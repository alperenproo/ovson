#include "AuroraService.h"
#include "../Net/Http.h"
#include <string>

namespace Aurora {

static bool findJsonBool(const std::string &json, const char *key, bool &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t c = json.find(':', k);
  if (c == std::string::npos)
    return false;
  size_t end = c + 1;
  while (end < json.size() && (json[end] == ' ' || json[end] == '\t'))
    ++end;
  if (end + 4 <= json.size() && json.substr(end, 4) == "true") {
    out = true;
    return true;
  }
  if (end + 5 <= json.size() && json.substr(end, 5) == "false") {
    out = false;
    return true;
  }
  return false;
}

static bool findJsonString(const std::string &json, const char *key,
                           std::string &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t q1 = json.find('"', json.find(':', k));
  if (q1 == std::string::npos)
    return false;
  size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos)
    return false;
  out = json.substr(q1 + 1, q2 - (q1 + 1));
  return true;
}

static bool findJsonInt(const std::string &json, const char *key, int &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t c = json.find(':', k);
  if (c == std::string::npos)
    return false;
  size_t end = c + 1;
  while (end < json.size() && (json[end] == ' ' || json[end] == '\t'))
    ++end;
  size_t e = end;
  if (e < json.size() && json[e] == '-')
    ++e;
  while (e < json.size() && isdigit((unsigned char)json[e]))
    ++e;
  if (e == end)
    return false;
  out = atoi(json.substr(end, e - end).c_str());
  return true;
}

static bool findJsonLong(const std::string &json, const char *key,
                         long long &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t c = json.find(':', k);
  if (c == std::string::npos)
    return false;
  size_t end = c + 1;
  while (end < json.size() && (json[end] == ' ' || json[end] == '\t'))
    ++end;
  size_t e = end;
  if (e < json.size() && json[e] == '-')
    ++e;
  while (e < json.size() && isdigit((unsigned char)json[e]))
    ++e;
  if (e == end)
    return false;
  try {
    out = std::stoll(json.substr(end, e - end));
  } catch (...) {
    return false;
  }
  return true;
}

std::optional<QueryResult> queryStats(const std::string &type,
                                      const std::string &value, int range,
                                      int maxResults,
                                      const std::string &apiKey) {
  if (apiKey.empty() || value.empty() || type.empty())
    return std::nullopt;

  std::string endpoint = (type == "beds") ? "beds" : "finals";
  std::string url = "https://bordic.xyz/api/v2/resources/lookup/" + endpoint +
                    "?key=" + apiKey + "&value=" + value +
                    "&range=" + std::to_string(range) +
                    "&max=" + std::to_string(maxResults);

  std::string body;
  bool ok = Http::get(url, body);
  if (!ok || body.empty())
    return std::nullopt;

  QueryResult result;
  result.success = false;

  findJsonBool(body, "success", result.success);
  if (!result.success && body.find("[") != 0)
    return std::nullopt;

  if (body.find("[") == 0)
    result.success = true;

  size_t arrStart = body.find('[');
  if (arrStart == std::string::npos)
    return result;

  int depth = 1;
  size_t i = arrStart + 1;
  while (i < body.size() && depth > 0) {
    if (body[i] == '[')
      depth++;
    else if (body[i] == ']')
      depth--;
    i++;
  }
  if (depth != 0)
    return result;

  std::string arrJson = body.substr(arrStart, i - arrStart);

  size_t pos = 0;
  while ((pos = arrJson.find('{', pos)) != std::string::npos) {
    size_t objEnd = arrJson.find('}', pos);
    if (objEnd == std::string::npos)
      break;
    std::string obj = arrJson.substr(pos, objEnd - pos + 1);

    PlayerMatch match;
    match.distance = -1;
    findJsonString(obj, "name", match.name);
    findJsonInt(obj, "distance", match.distance);
    if (match.distance == -1)
      findJsonInt(obj, "diff", match.distance);

    if (!match.name.empty()) {
      result.data.push_back(match);
    }
    pos = objEnd + 1;
  }

  return result;
}

std::optional<PingResult> queryPingHistory(const std::string &uuid,
                                           const std::string &apiKey) {
  if (apiKey.empty() || uuid.empty())
    return std::nullopt;

  std::string url = "https://bordic.xyz/api/v2/resources/ping?key=" + apiKey +
                    "&uuid=" + uuid;
  std::string body;
  bool ok = Http::get(url, body);
  if (!ok || body.empty())
    return std::nullopt;

  PingResult result;
  result.success = false;
  findJsonBool(body, "success", result.success);
  if (!result.success)
    return std::nullopt;

  size_t arrStart = body.find('[');
  if (arrStart == std::string::npos)
    return result;

  size_t pos = arrStart;
  while ((pos = body.find('{', pos)) != std::string::npos) {
    size_t objEnd = body.find('}', pos);
    if (objEnd == std::string::npos)
      break;
    std::string obj = body.substr(pos, objEnd - pos + 1);

    PingEntry entry;
    entry.min = -1;
    entry.avg = -1;
    entry.max = -1;
    entry.timestamp = 0;
    findJsonInt(obj, "min", entry.min);
    findJsonInt(obj, "avg", entry.avg);
    findJsonInt(obj, "max", entry.max);
    findJsonLong(obj, "timestamp", entry.timestamp);

    if (entry.avg != -1) {
      result.data.push_back(entry);
    }
    pos = objEnd + 1;
    if (pos >= body.rfind(']'))
      break;
  }

  return result;
}

} // namespace Aurora
