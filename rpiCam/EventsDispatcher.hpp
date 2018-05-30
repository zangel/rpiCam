#pragma once

#include "Config.hpp"

namespace rpiCam
{
    template <typename Events>
    class EventsDispatcher
    {
    protected:
        using ThisType = EventsDispatcher<Events>;
        using Subscriber = Events*;
        using SubscriberList = std::list<Subscriber>;

    public:
        EventsDispatcher()
          : m_Mutex()
          , m_Subscribers()
        {

        }

        ~EventsDispatcher()
        {
        }

        void operator+=(Subscriber subscriber) const
        {
          const_cast<ThisType*>(this)->add(subscriber);
        }

        void operator-=(Subscriber subscriber) const
        {
          const_cast<ThisType*>(this)->remove(subscriber);
        }

        template <typename Event, typename ...Args>
        void dispatch(Event event, Args &&... args)
        {
          SubscriberList subscribers;
          {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            subscribers = m_Subscribers;
          }

          for(auto subscriber : subscribers)
          {
            (subscriber->*event)(args...);
          };
        }

    private:
        void add(Subscriber subscriber)
        {
          std::lock_guard<std::recursive_mutex> lock(m_Mutex);
          m_Subscribers.push_front(subscriber);
        }

        void remove(Subscriber subscriber)
        {
          std::lock_guard<std::recursive_mutex> lock(m_Mutex);
          typename SubscriberList::iterator itSubscriber = std::find(
            m_Subscribers.begin(),
            m_Subscribers.end(),
            subscriber
          );

          if(itSubscriber != m_Subscribers.end())
            m_Subscribers.erase(itSubscriber);
        }

    private:
        mutable std::recursive_mutex m_Mutex;
        mutable SubscriberList m_Subscribers;
    };
}
