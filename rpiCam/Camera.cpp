#include "Camera.hpp"

namespace rpiCam
{
    Camera::Camera()
        : m_CameraEvents()
    {

    }


    Camera::~Camera()
    {

    }
    
    std::istream& operator>>(std::istream &s, Camera::AWBMode &v)
    {
        std::string sv;
        s >> sv;
        if (sv == "Off")
            v = Camera::AWBMode::Off;
        else if (sv == "Auto")
            v = Camera::AWBMode::Auto;
        else if (sv == "Sunlight")
            v = Camera::AWBMode::Sunlight;
        else if (sv == "Cloudy")
            v = Camera::AWBMode::Cloudy;
        else if (sv == "Shade")
            v = Camera::AWBMode::Shade;
        else if (sv == "Tungsten")
            v = Camera::AWBMode::Tungsten;
        else if (sv == "Fluorescent")
            v = Camera::AWBMode::Fluorescent;
        else if (sv == "Incandescent")
            v = Camera::AWBMode::Incandescent;
        else if (sv == "Flash")
            v = Camera::AWBMode::Flash;
        else if (sv == "Horizon")
            v = Camera::AWBMode::Horizon;
        else
            throw std::invalid_argument("Invalid value for AWBMode: " + sv);
        return s;
    }

    std::ostream& operator<<(std::ostream &s, Camera::AWBMode v)
    {
        switch(v)
        {
        case Camera::AWBMode::Off: 			s << "Off"; 			break;
        case Camera::AWBMode::Auto:			s << "Auto";			break;
        case Camera::AWBMode::Sunlight:		s << "Sunlight";		break;
        case Camera::AWBMode::Cloudy:		s << "Cloudy";			break;
        case Camera::AWBMode::Shade:		s << "Shade";			break;
        case Camera::AWBMode::Tungsten:		s << "Tungsten";		break;
        case Camera::AWBMode::Fluorescent:	s << "Fluorescent";		break;
        case Camera::AWBMode::Incandescent:	s << "Incandescent";	break;
        case Camera::AWBMode::Flash:		s << "Flash";			break;
        case Camera::AWBMode::Horizon:		s << "Horizon";			break;
        }
        return s;
    }

    std::istream& operator>>(std::istream &s, Camera::ExposureMode &v)
    {
        std::string sv;
        s >> sv;
        if (sv == "Off")
            v = Camera::ExposureMode::Off;
        else if (sv == "Auto")
            v = Camera::ExposureMode::Auto;
        else if (sv == "Night")
            v = Camera::ExposureMode::Night;
        else if (sv == "NightPreview")
            v = Camera::ExposureMode::NightPreview;
        else if (sv == "Backlight")
            v = Camera::ExposureMode::Backlight;
        else if (sv == "Spotlight")
            v = Camera::ExposureMode::Spotlight;
        else if (sv == "Sports")
            v = Camera::ExposureMode::Sports;
        else if (sv == "Snow")
            v = Camera::ExposureMode::Snow;
        else if (sv == "Beach")
            v = Camera::ExposureMode::Beach;
        else if (sv == "VeryLong")
            v = Camera::ExposureMode::VeryLong;
        else if (sv == "FixedFPS")
            v = Camera::ExposureMode::FixedFPS;
        else if (sv == "AntiShake")
            v = Camera::ExposureMode::AntiShake;
        else if (sv == "Fireworks")
            v = Camera::ExposureMode::Fireworks;
        else
            throw std::invalid_argument("Invalid value for ExposureMode: " + sv);
        return s;
    }

    std::ostream& operator<<(std::ostream &s, Camera::ExposureMode v)
    {
        switch(v)
        {
        case Camera::ExposureMode::Off:				s << "Off";				break;
        case Camera::ExposureMode::Auto:			s << "Auto";			break;
        case Camera::ExposureMode::Night:			s << "Night";			break;
        case Camera::ExposureMode::NightPreview:	s << "NightPreview";	break;
        case Camera::ExposureMode::Backlight:		s << "Backlight";		break;
        case Camera::ExposureMode::Spotlight:		s << "Spotlight";		break;
        case Camera::ExposureMode::Sports:			s << "Sports";			break;
        case Camera::ExposureMode::Snow:			s << "Snow";			break;
        case Camera::ExposureMode::Beach:			s << "Beach";			break;
        case Camera::ExposureMode::VeryLong:		s << "VeryLong";		break;
        case Camera::ExposureMode::FixedFPS:		s << "FixedFPS";		break;
        case Camera::ExposureMode::AntiShake:		s << "AntiShake";		break;
        case Camera::ExposureMode::Fireworks:		s << "Fireworks";		break;
        }
        return s;
    }

    std::istream& operator>>(std::istream &s, Camera::ExposureMeteringMode &v)
    {
        std::string sv;
        s >> sv;
        if (sv == "Average")
            v = Camera::ExposureMeteringMode::Average;
        else if (sv == "Spot")
            v = Camera::ExposureMeteringMode::Spot;
        else if (sv == "Backlit")
            v = Camera::ExposureMeteringMode::Backlit;
        else if (sv == "Matrix")
            v = Camera::ExposureMeteringMode::Matrix;
        else
            throw std::invalid_argument("Invalid value for ExposureMeteringMode: " + sv);
        return s;
    }

    std::ostream& operator<<(std::ostream &s, Camera::ExposureMeteringMode v)
    {
        switch(v)
        {
        case Camera::ExposureMeteringMode::Average:		s << "Average";		break;
        case Camera::ExposureMeteringMode::Spot:		s << "Spot";		break;
        case Camera::ExposureMeteringMode::Backlit:		s << "Backlit";		break;
        case Camera::ExposureMeteringMode::Matrix:		s << "Matrix";		break;
        }
        return s;
    }

    std::istream& operator>>(std::istream &s, Camera::DRCStrength &v)
    {
        std::string sv;
        s >> sv;
        if (sv == "Off")
            v = Camera::DRCStrength::Off;
        else if (sv == "Low")
            v = Camera::DRCStrength::Low;
        else if (sv == "Medium")
            v = Camera::DRCStrength::Medium;
        else if (sv == "High")
            v = Camera::DRCStrength::High;
        else
            throw std::invalid_argument("Invalid value for DRCStrength: " + sv);
        return s;
    }

    std::ostream& operator<<(std::ostream &s, Camera::DRCStrength v)
    {
        switch(v)
        {
        case Camera::DRCStrength::Off:			s << "Off";			break;
        case Camera::DRCStrength::Low:			s << "Low";			break;
        case Camera::DRCStrength::Medium:		s << "Medium";		break;
        case Camera::DRCStrength::High:			s << "High";		break;
        }
        return s;
    }

    std::istream& operator>>(std::istream &s, Camera::FlickerAvoid &v)
    {
        std::string sv;
        s >> sv;
        if (sv == "Off")
            v = Camera::FlickerAvoid::Off;
        else if (sv == "Auto")
            v = Camera::FlickerAvoid::Auto;
        else if (sv == "50Hz")
            v = Camera::FlickerAvoid::At50Hz;
        else if (sv == "60Hz")
            v = Camera::FlickerAvoid::At60Hz;
        else
            throw std::invalid_argument("Invalid value for DRCStrength: " + sv);
        return s;
    }

    std::ostream& operator<<(std::ostream &s, Camera::FlickerAvoid v)
    {
        switch(v)
        {
        case Camera::FlickerAvoid::Off:			s << "Off";		break;
        case Camera::FlickerAvoid::Auto:		s << "Auto";	break;
        case Camera::FlickerAvoid::At50Hz:		s << "50Hz";	break;
        case Camera::FlickerAvoid::At60Hz:		s << "60Hz";	break;
        }
        return s;
    }
}
