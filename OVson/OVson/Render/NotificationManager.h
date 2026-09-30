#pragma once
#include <Windows.h>
#include <mutex>
#include <string>
#include <vector>


namespace Render {
enum class NotificationType { Info, Success, Warning, Error };

struct NotificationSegment {
  std::string text;
  DWORD color = 0xFFE0E0E0;
};

struct Notification {
  std::string title;
  std::string message;
  NotificationType type;
  float timer;
  float duration;
  float slideAnim;
  std::vector<NotificationSegment> segments;

  DWORD getTitleColor() const;
  DWORD getBodyColor() const;
};

class NotificationManager {
public:
  static NotificationManager *getInstance();

  void add(const std::string &title, const std::string &message,
           NotificationType type = NotificationType::Info,
           float duration = 3.0f, std::size_t maximumVisible = 20);
  void addRich(const std::string &title,
               const std::vector<NotificationSegment> &segments,
               NotificationType type = NotificationType::Info,
               float duration = 3.0f, std::size_t maximumVisible = 20);
  void render(HDC hdc);

private:
  NotificationManager() = default;
  std::vector<Notification> m_notifications;
  std::mutex m_mutex;

  bool m_fontInit = false;
};
} // namespace Render
