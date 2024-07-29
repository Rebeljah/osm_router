// adapted from https://github.com/Rebeljah/pubsub (Jarrod Humpel 2024)

#pragma once

#include <variant>
#include <string>
#include <map>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>

#include <SFML/System.hpp>

using std::map;
using std::string;
using std::unordered_set;

namespace ps
{
    enum class EventType : int
    {
        MapDataLoaded,   // n/a
        NavBoxSubmitted, // NavBoxForm
        RouteCompleted   // CompleteRoute
    };

    struct Event
    {
        Event(EventType type) : type(type) {}

        EventType type;

        /**
         * Contains data from the navbox form
         */
        struct NavBoxForm
        {
            NavBoxForm(sf::Vector2<double> origin, sf::Vector2<double> destination, int algoName)
                : origin(origin), destination(destination), algoName(algoName) {}
            sf::Vector2<double> origin;
            sf::Vector2<double> destination;
            int algoName;
        };

        /**
         * Contains data about a calculated route
         */
        struct CompleteRoute
        {
            CompleteRoute(std::vector<int> edgeIndices, std::chrono::duration<double> runTime)
            : edgeIndices(edgeIndices), runTime(runTime) {}

            std::vector<int> edgeIndices;
            std::chrono::duration<double> runTime;
        };

        // refer to enum to check which member to access
        std::variant<std::monostate, NavBoxForm, CompleteRoute> data;
    };

    class ISubscriber;  // fwd declaration

    /**
     * pub/sub publisher class.
     */
    class Publisher
    {
    public:
        virtual ~Publisher();
        virtual void addSubscriber(ISubscriber *subscriber, EventType eventType);
        virtual void removeSubscriber(EventType eventType, ISubscriber *subscriber);

    protected:
        virtual void emitEvent(const Event &event);

    private:
        using SubscriberSet = unordered_set<ISubscriber *>;
        using SubscriberMap = map<EventType, SubscriberSet>;
        SubscriberMap m_subscribers;
    };

    class ISubscriber
    {
    public:
        virtual ~ISubscriber();
        virtual void trackPublisher(EventType, Publisher *);
        virtual void stopTrackingPublisher(EventType, Publisher *);
        virtual void onEvent(const Event &) = 0;

    private:
        using PublisherSet = unordered_set<Publisher *>;
        using PublisherMap = map<EventType, PublisherSet>;
        PublisherMap m_publishers;
    };

    /**
     * Threadsafe event queue using the pubsub pattern.
     *
     */
    class EventQueue : public ps::ISubscriber
    {
    public:
        /**
         * Should not normally be called manually, this is the function that a ps::Publisher
         * will invoke when emitting a event.
         */
        void onEvent(const ps::Event &event)
        {
            pushEvent(event);
        }

        /*
        Push a event into the queue. threadsafe.
        */
        void pushEvent(const ps::Event &event)
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            queue.push(event);
        }

        /**
         * Pop the next event from the queue and return it.
         */
        ps::Event popNext()
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            ps::Event res = queue.front();
            queue.pop();
            return res;
        }

        /**
         * Returns true to indicate the queue is empty.
         */
        bool empty()
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            return queue.empty();
        }

        /**
         * Get current size
         */
        size_t size()
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            return queue.size();
        }

    private:
        std::queue<ps::Event> queue;
        std::mutex lock;
    };
}; // namespace pubsub

ps::Publisher::~Publisher()
{
    for (auto kv : m_subscribers)
    {
        EventType eventType = kv.first;
        SubscriberSet &subscribers = kv.second;
        for (auto pSubscriber : subscribers)
        {
            pSubscriber->stopTrackingPublisher(eventType, this);
        }
    }
}

void ps::Publisher::addSubscriber(ISubscriber *subscriber, EventType eventType)
{
    // init subscriber set for this event type
    if (m_subscribers.find(eventType) == m_subscribers.end())
    {
        m_subscribers.emplace(eventType, SubscriberSet());
    }

    m_subscribers.at(eventType).emplace(subscriber);
    subscriber->trackPublisher(eventType, this);
}

void ps::Publisher::removeSubscriber(EventType eventType, ISubscriber *subscriber)
{
    auto &subscriberSet = m_subscribers.at(eventType);
    subscriberSet.erase(subscriber);

    // erase this event type from map if subscriber was last subscriber
    if (subscriberSet.empty())
        m_subscribers.erase(eventType);
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

ps::ISubscriber::~ISubscriber()
{
    for (auto kv : m_publishers)
    {
        EventType eventType = kv.first;
        PublisherSet &publishers = kv.second;
        for (auto pPublisher : publishers)
        {
            pPublisher->removeSubscriber(eventType, this);
        }
    }
}

void ps::ISubscriber::trackPublisher(EventType eventType, Publisher *publisher)
{
    if (m_publishers.find(eventType) == m_publishers.end())
    {
        m_publishers.emplace(eventType, PublisherSet());
    }

    m_publishers.at(eventType).emplace(publisher);
}

void ps::ISubscriber::stopTrackingPublisher(EventType eventType, Publisher *publisher)
{
    auto &publisherSet = m_publishers.at(eventType);
    publisherSet.erase(publisher);

    // erase this event type from map if subscriber was last subscriber
    if (publisherSet.empty())
        m_publishers.erase(eventType);
}
