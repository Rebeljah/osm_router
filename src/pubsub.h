// pubsub code adapted from https://github.com/Rebeljah/pubsub (Jarrod Humpel 2024)

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
    /**
     * Different types of events that can be emitted and listened to. The comments
     * next to each enum member denote which data type will be assigned to the
     * `data` member of the Event that is emitted. Each event type is associated with
     * 0 or 1 data types. Event types that do not contain data are denoted with "n/a"
     */
    enum class EventType : int
    {
        MapDataLoaded,   // n/a
        NavBoxSubmitted, // NavBoxForm
        NavBoxFormChanged, // NavBoxForm
        RouteCompleted,  // CompleteRoute
        EdgeAnimated,    // AnimatedEdge
    };

    namespace Data
    {
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

        /**
         * Contains data about an edge to be animated
         */
        struct AnimatedEdge
        {
            AnimatedEdge(std::vector<int> edgeIndices) : edgeIndices(edgeIndices) {}
            std::vector<int> edgeIndices;
        };
    };

    struct Event
    {
        /**
         * Refer to EventType enum to determine which data type to access for a given
         * event. Example access: If the event type is NavBoxSubmitted and the NavBoxForm datatype
         * is associated with that event type then the form data can be accessed like this
         * NavBoxForm form = std::get<NavBoxForm>(event.data);.
         */

        Event(EventType type) : type(type) {}

        EventType type;
        std::variant<std::monostate, Data::NavBoxForm, Data::CompleteRoute, Data::AnimatedEdge> data;
    };

    class ISubscriber; // fwd declaration

    /**
     * pub/sub publisher class can be subclassed or used by itself. The publisher
     * emits events which Subscribers can listen to.
     */
    class Publisher
    {
    public:
        virtual ~Publisher();

        /**
         * Register the Subscriber to this Publisher to listen for a specific
         * event type.
         * @param subscriber A pointer to the Subscriber instance that needs to
         * listen for events emitted by this Publisher.
         * @param eventType The type of event that the subscriber is interested in
         * being notified about
         */
        virtual void addSubscriber(ISubscriber *subscriber, EventType eventType);

        /**
         * Should be called when the Subscriber no longer wants to receive events
         * from this Publisher with the given event type.
         * @param subscriber The subscriber instance to stop notifying
         * @param eventType The event type that the subscriber will no longer receive
         * event notification for.
         */
        virtual void removeSubscriber(ISubscriber *subscriber, EventType eventType);

        /**
         * Check whether the given subscriber is listening to the event type
         * from this publisher
         * @param subscriber The subscriber to check
         * @param eventType the type of event to check if the subscriber is listening
         * to.
         * @returns true if the subscriber is listening the the given event type
         * from this publisher, false otherwise.
         */
        virtual bool hasSubscriber(ISubscriber *subscriber, EventType eventType);

    protected:
        /**
         * Emit an event, notifying all subscribers of this publisher that the event
         * was published if the subscriber is listening for the specific event type
         * @param event The event to be emitted. may or may not contain meaningful
         * extra data.
         */
        virtual void emitEvent(const Event &event);

    private:
        using SubscriberSet = std::unordered_set<ISubscriber *>;
        using SubscriberMap = std::map<EventType, SubscriberSet>;
        SubscriberMap m_subscribers;
    };

    /**
     * Abstract class representing a subscriber in a pub/sub system. Subscribers
     * can subscribe to specific event types from publishers and handle those events.
     */
    class ISubscriber
    {
    public:
        virtual ~ISubscriber();

        /**
         * Start listening to events with the given type emitted by the publisher.
         * @param publisher The publisher to listen to
         * @param eventType the event type to listen to from the publisher.
         */
        virtual void subscribe(Publisher *publisher, EventType eventType);

        /**
         * Stop listening to the event type from the given publisher.
         * @param publisher The publisher to unsubscribe from
         * @param eventType the event type to stop listening for.
         */
        virtual void unsubscribe(Publisher *publisher, EventType eventType);

        /**
         * Check if this subscriber is subscribed to the given publisher with the given
         * event type.
         * @param publisher The publisher to check
         * @param eventType the event type associated with the subscription
         * @returns true if the subscriber is listening to the event type from the
         * publisher, false otherwise.
         */
        virtual bool isSubscribedTo(Publisher *publisher, EventType eventType);

        /**
         * Handle an event emitted by a publisher. This method needs to be implemented
         * by concrete subclasses.
         * @param event The event to handle
         */
        virtual void onEvent(const Event &event) = 0;

    private:
        using PublisherSet = std::unordered_set<Publisher *>;
        using PublisherMap = std::map<EventType, PublisherSet>;
        PublisherMap m_publishers;
    };

    /**
     * Threadsafe event queue using the pubsub pattern.
     */
    class EventQueue : public ps::ISubscriber
    {
    public:
        /**
         * Should not normally be called manually, this is the function that a ps::Publisher
         * will invoke when emitting an event.
         */
        void onEvent(const ps::Event &event) override;

        /**
         * Push an event into the queue. Threadsafe.
         * @param event the event to queue
         */
        void pushEvent(const ps::Event &event);

        /**
         * Pop the next event from the queue and return it.
         */
        ps::Event popNext();

        /**
         * Returns true to indicate the queue is empty.
         */
        bool empty() const;

        /**
         * Get current size.
         */
        size_t size() const;

    private:
        std::queue<ps::Event> queue;
        mutable std::mutex lock;
    };
}; // namespace pubsub