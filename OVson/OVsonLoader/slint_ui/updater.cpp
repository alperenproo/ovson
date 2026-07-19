// See updater.h for the design overview.

#include "updater.h"

#include <Windows.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace Updater {

const char *kBuildVersion = "3.0.4";

const char *kRepoOwner    = "alperenproo";
const char *kRepoName     = "ovson";
const char *kAssetName    = "OVsonLoader.exe";

namespace {
std::atomic<State>   s_state{State::Idle};
std::atomic<int>     s_downloadPct{0};
std::mutex           s_infoMutex;
Info                 s_info;
std::string          s_lastError;

void setError(const std::string &msg) {
  std::lock_guard<std::mutex> lk(s_infoMutex);
  s_lastError = msg;
  s_state.store(State::Failed);
}

std::string httpGet(const std::wstring &host, const std::wstring &path,
                    bool followRedirects = false) {
  std::string out;
  HINTERNET hSession = WinHttpOpen(
      L"OVsonLoader-Updater/1.0",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) { setError("WinHttpOpen failed"); return out; }

  HINTERNET hConnect = WinHttpConnect(
      hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    setError("WinHttpConnect failed");
    WinHttpCloseHandle(hSession); return out;
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", path.c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
      WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    setError("WinHttpOpenRequest failed");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return out;
  }

  if (followRedirects) {
    DWORD flags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                     &flags, sizeof(flags));
  }

  WinHttpAddRequestHeaders(
      hRequest, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L,
      WINHTTP_ADDREQ_FLAG_ADD);

  BOOL ok = WinHttpSendRequest(hRequest,
                               WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (!ok) {
    setError("WinHttpSendRequest failed");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return out;
  }
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    setError("WinHttpReceiveResponse failed");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return out;
  }

  DWORD bytesAvail = 0;
  do {
    bytesAvail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvail)) break;
    if (bytesAvail == 0) break;
    std::string chunk((size_t)bytesAvail, '\0');
    DWORD bytesRead = 0;
    if (!WinHttpReadData(hRequest, &chunk[0], bytesAvail, &bytesRead))
      break;
    chunk.resize(bytesRead);
    out.append(chunk);
  } while (bytesAvail > 0);

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return out;
}

bool httpDownloadToFile(const std::wstring &host,
                       const std::wstring &path,
                       const std::wstring &outFile,
                       std::atomic<int> &pctSink) {
  HINTERNET hSession = WinHttpOpen(
      L"OVsonLoader-Updater/1.0",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) { setError("WinHttpOpen failed"); return false; }

  HINTERNET hConnect = WinHttpConnect(
      hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    setError("WinHttpConnect failed");
    WinHttpCloseHandle(hSession); return false;
  }

  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", path.c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
      WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    setError("WinHttpOpenRequest failed");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return false;
  }
  DWORD flags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                   &flags, sizeof(flags));

  if (!WinHttpSendRequest(hRequest,
                          WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(hRequest, nullptr)) {
    setError("WinHttp send/receive failed");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return false;
  }

  DWORD totalSize = 0;
  DWORD sz = sizeof(totalSize);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_CONTENT_LENGTH |
                          WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &totalSize, &sz,
                      WINHTTP_NO_HEADER_INDEX);

  std::ofstream out(outFile, std::ios::binary);
  if (!out) {
    setError("Cannot open output file for download");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession); return false;
  }

  DWORD got = 0;
  std::vector<char> buf(64 * 1024);
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
    if (avail == 0) break;
    if (avail > buf.size()) buf.resize(avail);
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, buf.data(), avail, &read)) break;
    out.write(buf.data(), read);
    got += read;
    if (totalSize > 0) {
      int pct = (int)((double)got / (double)totalSize * 100.0);
      if (pct > 99) pct = 99;
      pctSink.store(pct);
    }
  }
  out.close();
  pctSink.store(100);

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return true;
}

std::string jsonString(const std::string &body, const std::string &key) {
  std::string needle = "\"" + key + "\"";
  auto p = body.find(needle);
  if (p == std::string::npos) return "";
  p = body.find(':', p);
  if (p == std::string::npos) return "";
  while (++p < body.size() && (body[p] == ' ' || body[p] == '\t')) {}
  if (p >= body.size() || body[p] != '"') return "";
  ++p;
  std::string out;
  while (p < body.size() && body[p] != '"') {
    if (body[p] == '\\' && p + 1 < body.size()) {
      char c = body[p + 1];
      switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case '"': out += '"';  break;
        case '\\': out += '\\'; break;
        default:  out += c;    break;
      }
      p += 2;
    } else {
      out += body[p++];
    }
  }
  return out;
}

std::string jsonAssetUrlByName(const std::string &body,
                               const std::string &expectedName) {
  size_t pos = 0;
  while (true) {
    size_t nameAt = body.find("\"name\"", pos);
    if (nameAt == std::string::npos) return "";
    size_t colon = body.find(':', nameAt);
    if (colon == std::string::npos) return "";
    size_t startQuote = body.find('"', colon);
    if (startQuote == std::string::npos) return "";
    size_t endQuote = body.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return "";
    std::string thisName = body.substr(startQuote + 1,
                                       endQuote - startQuote - 1);
    if (thisName == expectedName) {
      size_t urlAt = body.find("\"browser_download_url\"", endQuote);
      if (urlAt == std::string::npos) return "";
      size_t urlColon = body.find(':', urlAt);
      size_t urlStart = body.find('"', urlColon);
      size_t urlEnd   = body.find('"', urlStart + 1);
      if (urlStart == std::string::npos || urlEnd == std::string::npos)
        return "";
      return body.substr(urlStart + 1, urlEnd - urlStart - 1);
    }
    pos = endQuote + 1;
  }
}

int compareVersions(const std::string &a, const std::string &b) {
  auto strip = [](const std::string &s) {
    return (!s.empty() && (s[0] == 'v' || s[0] == 'V'))
               ? s.substr(1) : s;
  };
  auto split = [](const std::string &s) {
    std::vector<int> parts;
    size_t pos = 0;
    while (pos < s.size()) {
      size_t dot = s.find('.', pos);
      if (dot == std::string::npos) dot = s.size();
      try { parts.push_back(std::stoi(s.substr(pos, dot - pos))); }
      catch (...) { parts.push_back(0); }
      pos = dot + 1;
    }
    return parts;
  };
  auto pa = split(strip(a));
  auto pb = split(strip(b));
  size_t n = pa.size() > pb.size() ? pa.size() : pb.size();
  for (size_t i = 0; i < n; i++) {
    int x = i < pa.size() ? pa[i] : 0;
    int y = i < pb.size() ? pb[i] : 0;
    if (x != y) return x < y ? -1 : 1;
  }
  return 0;
}

std::wstring runningExeDir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring p = path;
  size_t slash = p.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return L".\\";
  return p.substr(0, slash + 1);
}

std::wstring runningExePath() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return std::wstring(path);
}

} // anonymous namespace

State currentState() { return s_state.load(); }
int downloadPct()    { return s_downloadPct.load(); }

const Info &info() {
  std::lock_guard<std::mutex> lk(s_infoMutex);
  return s_info;
}
const std::string &lastError() {
  std::lock_guard<std::mutex> lk(s_infoMutex);
  return s_lastError;
}

void startCheck() {
  State expected = State::Idle;
  if (!s_state.compare_exchange_strong(expected, State::Checking))
    return; // already running
  std::thread([]() {
    std::wstring host = L"api.github.com";
    std::wstring path = L"/repos/";
    for (const char *p = kRepoOwner; *p; ++p) path += (wchar_t)*p;
    path += L"/";
    for (const char *p = kRepoName; *p; ++p)  path += (wchar_t)*p;
    path += L"/releases/latest";
    std::string body = httpGet(host, path);
    if (body.empty()) return;

    std::string tag = jsonString(body, "tag_name");
    std::string url = jsonAssetUrlByName(body, kAssetName);
    std::string notes = jsonString(body, "body");
    if (tag.empty()) {
      setError("No tag_name in GitHub response");
      return;
    }
    int cmp = compareVersions(kBuildVersion, tag);
    {
      std::lock_guard<std::mutex> lk(s_infoMutex);
      s_info.remoteVersion = tag;
      s_info.downloadUrl   = url;
      s_info.releaseNotes  = notes;
    }
    s_state.store(cmp < 0 ? State::UpdateAvailable : State::UpToDate);
  }).detach();
}

void startDownload() {
  State expected = State::UpdateAvailable;
  if (!s_state.compare_exchange_strong(expected, State::Downloading))
    return;
  s_downloadPct.store(0);
  std::thread([]() {
    std::string url;
    {
      std::lock_guard<std::mutex> lk(s_infoMutex);
      url = s_info.downloadUrl;
    }
    if (url.empty()) { setError("No asset URL"); return; }
    // Split URL: https://host/path?query
    if (url.rfind("https://", 0) != 0) {
      setError("Asset URL is not HTTPS"); return;
    }
    auto hostStart = 8;
    auto pathStart = url.find('/', hostStart);
    std::string host = url.substr(hostStart, pathStart - hostStart);
    std::string path = url.substr(pathStart);
    std::wstring whost(host.begin(), host.end());
    std::wstring wpath(path.begin(), path.end());
    std::wstring out = runningExePath() + L".new";
    if (!httpDownloadToFile(whost, wpath, out, s_downloadPct)) return;
    s_state.store(State::Ready);
  }).detach();
}

bool installAndRelaunch() {
  if (s_state.load() != State::Ready) return false;
  std::wstring exe = runningExePath();
  std::wstring dir = runningExeDir();
  std::wstring cmdPath = dir + L"ovson_update.cmd";

  std::string script;
  script += "@echo off\r\n";
  script += "setlocal EnableExtensions\r\n";
  script += "set \"PID=%~1\"\r\n";
  script += "set \"TARGET=%~2\"\r\n";
  script += "set \"STAGED=%~3\"\r\n";
  script += "timeout /t 2 /nobreak >NUL\r\n";
  script += ":wait\r\n";
  script += "tasklist /FI \"PID eq %PID%\" 2>NUL | find \"%PID%\" >NUL\r\n";
  script += "if errorlevel 1 goto exited\r\n";
  script += "timeout /t 1 /nobreak >NUL\r\n";
  script += "goto wait\r\n";
  script += ":exited\r\n";
  script += "if not exist \"%STAGED%\" goto cleanup\r\n";
  script += "del \"%TARGET%\" >NUL 2>&1\r\n";
  script += "if exist \"%TARGET%\" goto cleanup\r\n";
  script += "ren \"%STAGED%\" \"%~nx2\" >NUL 2>&1\r\n";
  script += "if not exist \"%TARGET%\" goto cleanup\r\n";
  script += "start \"\" /D \"%~dp2\" \"%TARGET%\"\r\n";
  script += ":cleanup\r\n";
  script += "(goto) 2>nul & del \"%~f0\"\r\n";

  HANDLE h = CreateFileW(cmdPath.c_str(), GENERIC_WRITE, 0, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    setError("Cannot write update.cmd"); return false;
  }
  DWORD wr = 0;
  WriteFile(h, script.data(), (DWORD)script.size(), &wr, nullptr);
  CloseHandle(h);

  wchar_t pidBuf[16];
  swprintf_s(pidBuf, L"%lu", GetCurrentProcessId());
  std::wstring cmdLine =
      L"cmd.exe /c \"\"" + cmdPath + L"\" " + pidBuf + L" \"" + exe + L"\" \"" + exe + L".new\"\"";

  STARTUPINFOW si{ sizeof(si) };
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> cmdLineMut(cmdLine.begin(), cmdLine.end());
  cmdLineMut.push_back(0);
  if (!CreateProcessW(nullptr, cmdLineMut.data(), nullptr, nullptr,
                      FALSE, CREATE_NO_WINDOW,
                      nullptr, dir.c_str(), &si, &pi)) {
    setError("CreateProcess(update.cmd) failed");
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

} // namespace Updater
