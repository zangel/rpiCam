#pragma once

#include "Config.hpp"
#include "EventsDispatcher.hpp"

namespace rpiCam
{
    class Device
    {
    public:
        class Events
        {
        public:
            virtual void onDeviceOpened() {}
            virtual void onDeviceClosed() {}
        };

        using DeviceEvents = EventsDispatcher<Events>;

        Device();
        virtual ~Device();

        virtual std::string const& name() const = 0;

        virtual bool isOpen() const = 0;
        virtual std::error_code open() = 0;
        virtual std::error_code close() = 0;

        inline DeviceEvents const& deviceEvents() { return m_DeviceEvents; }


        template <typename D>
        static std::list< std::shared_ptr<D> > list();

    protected:
        inline void dispatchOnDeviceOpened()
        {
            m_DeviceEvents.dispatch(&Events::onDeviceOpened);
        }

        inline void dispatchOnDeviceClosed()
        {
            m_DeviceEvents.dispatch(&Events::onDeviceClosed);
        }


    protected:
        DeviceEvents m_DeviceEvents;
    };
}

