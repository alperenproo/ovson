package net.ovson.api.event;

import java.lang.reflect.Method;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public final class EventBus {
    private static final EventBus INSTANCE = new EventBus();

    public static EventBus getInstance() {
        return INSTANCE;
    }

    private final Map<Class<? extends Event>, List<ListenerMethod>> listenerMap = new ConcurrentHashMap<>();

    private EventBus() {}

    public void register(Object listener) {
        for (Method method : listener.getClass().getDeclaredMethods()) {
            if (!method.isAnnotationPresent(EventHandler.class)) {
                continue;
            }
            if (method.getParameterCount() != 1) {
                continue;
            }
            Class<?> paramType = method.getParameterTypes()[0];
            if (!Event.class.isAssignableFrom(paramType)) {
                continue;
            }

            @SuppressWarnings("unchecked")
            Class<? extends Event> eventClass = (Class<? extends Event>) paramType;
            method.setAccessible(true);

            listenerMap.computeIfAbsent(eventClass, k -> new CopyOnWriteArrayList<>())
                       .add(new ListenerMethod(listener, method));
        }
    }

    public void unregister(Object listener) {
        for (List<ListenerMethod> list : listenerMap.values()) {
            list.removeIf(lm -> lm.instance == listener);
        }
    }

    public void post(Event event) {
        List<ListenerMethod> list = listenerMap.get(event.getClass());
        if (list == null) return;

        for (ListenerMethod lm : list) {
            try {
                lm.method.invoke(lm.instance, event);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private static class ListenerMethod {
        final Object instance;
        final Method method;

        ListenerMethod(Object instance, Method method) {
            this.instance = instance;
            this.method = method;
        }
    }
}
