#include "pubsub.h"

// PUBLISHER IMPL

ps::Publisher::~Publisher()
{
    for (auto &[eventType, subscribers] : m_subscribers)
    {
        for (auto subscriber : subscribers)
        {
            removeSubscriber(subscriber, eventType);
        }
    }
};

void ps::Publisher::addSubscriber(ISubscriber *subscriber, EventType eventType)
{
    // init subscriber set for this event type
    if (m_subscribers.find(eventType) == m_subscribers.end())
    {
        m_subscribers.emplace(eventType, SubscriberSet());
    }

    m_subscribers.at(eventType).emplace(subscriber);

    if (!subscriber->isSubscribedTo(this, eventType))
        subscriber->subscribe(this, eventType);
};

void ps::Publisher::removeSubscriber(ISubscriber *subscriber, EventType eventType)
{
    auto &subscriberSet = m_subscribers.at(eventType);
    subscriberSet.erase(subscriber);

    // erase this event type from map if subscriber was last subscriber
    if (subscriberSet.empty())
        m_subscribers.erase(eventType);

    if (subscriber->isSubscribedTo(this, eventType))
        subscriber->unsubscribe(this, eventType);
}

bool ps::Publisher::hasSubscriber(ISubscriber *subscriber, EventType eventType)
{
    auto it = m_subscribers.find(eventType);
    if (it == m_subscribers.end())
        return false;

    const SubscriberSet &subscribers = it->second;
    return subscribers.count(subscriber) == 1;
}

void ps::Publisher::emitEvent(const Event &event)
{
    if (m_subscribers.find(event.type) == m_subscribers.end())
        return; // event type has no subscribers so return

    for (auto pSubscriber : m_subscribers.at(event.type))
    {
        pSubscriber->onEvent(event);
    }
}

// SUBSCRIBER IMPL

ps::ISubscriber::~ISubscriber()
{
    for (auto &[eventType, publishers] : m_publishers)
    {
        for (auto publisher : publishers)
        {
            unsubscribe(publisher, eventType);
        }
    }
}

void ps::ISubscriber::subscribe(Publisher *publisher, EventType eventType)
{
    // Initialize publisher set for this event type
    if (m_publishers.find(eventType) == m_publishers.end())
    {
        m_publishers.emplace(eventType, PublisherSet());
    }

    m_publishers.at(eventType).emplace(publisher);

    if (!publisher->hasSubscriber(this, eventType))
        publisher->addSubscriber(this, eventType);
}

void ps::ISubscriber::unsubscribe(Publisher *publisher, EventType eventType)
{
    auto &publisherSet = m_publishers.at(eventType);
    publisherSet.erase(publisher);

    // Erase this event type from map if subscriber was last subscriber
    if (publisherSet.empty())
        m_publishers.erase(eventType);

    if (publisher->hasSubscriber(this, eventType))
        publisher->removeSubscriber(this, eventType);
}

bool ps::ISubscriber::isSubscribedTo(Publisher *publisher, EventType eventType)
{
    auto it = m_publishers.find(eventType);
    if (it == m_publishers.end())
        return false;

    const PublisherSet &publishers = it->second;
    return publishers.count(publisher) == 1;
}

// EVENT QUEUE IMPL

void ps::EventQueue::onEvent(const ps::Event &event)
{
    pushEvent(event);
}

void ps::EventQueue::pushEvent(const ps::Event &event)
{
    const std::lock_guard<std::mutex> lock_guard(lock);
    queue.push(event);
}

ps::Event ps::EventQueue::popNext()
{
    const std::lock_guard<std::mutex> lock_guard(lock);
    if (queue.empty())
    {
        throw std::runtime_error("Attempt to pop from an empty queue");
    }
    ps::Event res = queue.front();
    queue.pop();
    return res;
}

bool ps::EventQueue::empty() const
{
    std::lock_guard<std::mutex> lock_guard(lock);
    return queue.empty();
}

size_t ps::EventQueue::size() const
{
    std::lock_guard<std::mutex> lock_guard(lock);
    return queue.size();
}
