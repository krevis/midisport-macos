//
//  HardwareConfiguration.cpp
//  EZUSBFirmwareDownloader
//
//  Created by Leigh Smith on 17/5/2020.
//

#include "HardwareConfiguration.h"
#define MAX_PATH_LEN 256

HardwareConfiguration::HardwareConfiguration(const char *configFilePath)
{
    CFStringRef filePath = CFStringCreateWithCString(kCFAllocatorDefault, configFilePath, kCFStringEncodingUTF8);
    CFURLRef configFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePath, kCFURLPOSIXPathStyle, false);
    if (configFileURL) {
        if (!this->readConfigFile(configFileURL)) {
            // TODO Either raise an exception or some other signalling?
        }
        CFRelease(configFileURL);
    }
}

HardwareConfiguration::~HardwareConfiguration()
{
    // TODO free up the deviceList.
}

// Retrieve the DeviceFirmware structure for the given cold boot device id.
struct DeviceFirmware HardwareConfiguration::deviceFirmwareForBootId(unsigned int coldBootDeviceId)
{
    // Search for coldBootDeviceId
    return deviceList[coldBootDeviceId];
}

// Converts the parameters of a single MIDISPORT model, in a property list CFDictionary, into the C++ DeviceFirmware structure.
// This does the data validation that all the parameters are there, returning true or false.
bool HardwareConfiguration::deviceListFromDictionary(CFDictionaryRef deviceConfig, struct DeviceFirmware &deviceFirmware)
{
    // Name of device model
    CFTypeRef deviceNameString = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("DeviceName"));
    if (!deviceNameString)
        return false;
    if (CFGetTypeID(deviceNameString) == CFStringGetTypeID()) {
        char deviceName[MAX_PATH_LEN];

        if (CFStringGetCString((CFStringRef) deviceNameString, deviceName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            deviceFirmware.modelName = std::string(deviceName);
        }
        else {
            return false;
        }
    }
    // Firmware pathname
    CFTypeRef firmwareFileName = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("FilePath"));
    if (!firmwareFileName)
        return false;
    if (CFGetTypeID(firmwareFileName) == CFStringGetTypeID()) {
        char fileName[MAX_PATH_LEN];

        if (CFStringGetCString((CFStringRef) firmwareFileName, fileName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            deviceFirmware.firmwareFileName = std::string(fileName);
        }
        else {
            return false;
        }
    }
    // Name of cold boot product id.
    CFTypeRef coldBootProductId = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("ColdBootProductID"));
    if (CFGetTypeID(coldBootProductId) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) coldBootProductId, kCFNumberIntType, &deviceFirmware.coldBootProductID)) {
            return false;
        }
    }
    // Name of warm firmware product id.
    CFTypeRef warmFirmwareProductId = CFDictionaryGetValue((CFDictionaryRef) deviceConfig, CFSTR("WarmFirmwareProductID"));
    if (CFGetTypeID(warmFirmwareProductId) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef) warmFirmwareProductId, kCFNumberIntType, &deviceFirmware.warmFirmwareProductID)) {
            return false;
        }
    }
    return true;
}

// Read the XML property list file containing the declarations for the MIDISPORT devices.
bool HardwareConfiguration::readConfigFile(CFURLRef configFileURL)
{
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, configFileURL);
    
    if (stream == NULL)
        return false;
    if (CFReadStreamOpen(stream)) {
        // Reconstitute the dictionary using the XML data
        CFErrorRef errorCode;
        CFPropertyListFormat propertyListFormat;
        CFPropertyListRef configurationPropertyList = CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                                                                     stream, 0,
                                                                                     kCFPropertyListImmutable,
                                                                                     &propertyListFormat,
                                                                                     &errorCode);
        if (configurationPropertyList == NULL) {
            // Handle the error
            fprintf(stderr, "Unable to load configuration property list format %ld\n", (long) propertyListFormat);
            CFStringRef errorDescription = CFErrorCopyDescription(errorCode);
            CFShow(errorDescription);
            CFRelease(errorCode);
            return false;
        }

        if (CFGetTypeID(configurationPropertyList) != CFDictionaryGetTypeID()) {
            CFRelease(configurationPropertyList);
            return false;
        }
        // CFShow(configurationPropertyList); // Debugging.

        // Break out the parameters and populate the internal state.
        // Retrieve and save the hex loader file path.
        CFTypeRef hexloaderFilePathString = CFDictionaryGetValue((CFDictionaryRef) configurationPropertyList, CFSTR("HexLoader"));
        if (!hexloaderFilePathString)
            return false;

        if (CFGetTypeID(hexloaderFilePathString) != CFStringGetTypeID()) {
            CFRelease(hexloaderFilePathString);
            return false;
        }
        char fileName[MAX_PATH_LEN];
        if (CFStringGetCString((CFStringRef) hexloaderFilePathString, fileName, MAX_PATH_LEN, kCFStringEncodingUTF8)) {
            hexloaderFilePathName = std::string(fileName);
        }

        // Verify there is a device list:
        CFTypeRef deviceArray = CFDictionaryGetValue((CFDictionaryRef) configurationPropertyList, CFSTR("Devices"));
        if (CFGetTypeID(deviceArray) != CFArrayGetTypeID()) {
            return false;
        }
        
        // Loop over the device list:
        for (CFIndex deviceIndex = 0; deviceIndex < CFArrayGetCount((CFArrayRef) deviceArray); deviceIndex++) {
            CFDictionaryRef deviceConfig = (CFDictionaryRef) CFArrayGetValueAtIndex((CFArrayRef) deviceArray, deviceIndex);

            if (CFGetTypeID(deviceConfig) == CFDictionaryGetTypeID()) {
                struct DeviceFirmware deviceFirmware;

                if (!this->deviceListFromDictionary(deviceConfig, deviceFirmware))
                    return false;
                deviceList[deviceFirmware.coldBootProductID] = deviceFirmware;
            }
        }
        CFRelease(configurationPropertyList);
    }
    CFRelease(stream);
    return true;
}
