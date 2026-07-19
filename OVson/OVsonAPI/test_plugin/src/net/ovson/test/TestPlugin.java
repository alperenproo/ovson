package net.ovson.test;

import net.ovson.api.OVsonPlugin;
import net.ovson.api.PluginInfo;
import net.ovson.api.chat.ChatAPI;
import net.ovson.api.event.EventHandler;
import net.ovson.api.event.EventBus;
import net.ovson.api.event.ChatReceivedEvent;
import net.ovson.api.event.ChatSendEvent;
import java.util.List;

@PluginInfo(
    name = "OVsonTest",
    version = "1.0",
    author = "TestAuthor",
    description = "Test plugin for checking JNI and event system."
)
public class TestPlugin extends OVsonPlugin {

    @Override
    public void onEnable() {
        getLogger().info("TestPlugin enabled successfully!");
        ChatAPI.showPrefixedMessage("§aTestPlugin loaded and activated successfully!");
        
        // Register events to EventBus
        EventBus.getInstance().register(this);
    }

    @Override
    public void onDisable() {
        getLogger().info("TestPlugin disabled!");
        // Unregister events
        EventBus.getInstance().unregister(this);
    }

    @EventHandler
    public void onChat(ChatReceivedEvent event) {
        String msg = event.getMessage();
        if (msg.contains("hello ovson")) {
            ChatAPI.showPrefixedMessage("§bHello! TestPlugin is active.");
        }
    }

    @EventHandler
    public void onChatSend(ChatSendEvent event) {
        String msg = event.getMessage();
        if (msg.startsWith(".test ")) {
            event.setCancelled(true); // Prevent message from going to the server
            String[] args = msg.split(" ");
            if (args.length < 2) return;

            String subCommand = args[1].toLowerCase();
            switch (subCommand) {
                case "title":
                    ChatAPI.showTitle("§c§lOVson", "§eTitle Test", 20, 60, 20);
                    ChatAPI.showPrefixedMessage("§aTitle sent to the screen.");
                    break;
                case "actionbar":
                    ChatAPI.showActionBar("§bActionbar Test Active!");
                    ChatAPI.showPrefixedMessage("§aActionbar message sent.");
                    break;
                case "clear":
                    ChatAPI.clearChat();
                    ChatAPI.showPrefixedMessage("§aChat cleared.");
                    break;
                case "history":
                    List<String> hist = ChatAPI.getChatHistory(5);
                    ChatAPI.showPrefixedMessage("§6Last 5 Received Messages:");
                    for (int i = 0; i < hist.size(); i++) {
                        ChatAPI.showClientMessage("§7" + (i + 1) + ". " + hist.get(i));
                    }
                    break;
                case "sent":
                    List<String> sent = ChatAPI.getSentHistory(5);
                    ChatAPI.showPrefixedMessage("§6Last 5 Sent Commands/Messages:");
                    for (int i = 0; i < sent.size(); i++) {
                        ChatAPI.showClientMessage("§7" + (i + 1) + ". " + sent.get(i));
                    }
                    break;
                default:
                    ChatAPI.showPrefixedMessage("§cAvailable subcommands: title, actionbar, clear, history, sent");
                    break;
            }
        }
    }
}

