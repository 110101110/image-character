#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "../include/NativeFileDialog.h"

std::string OpenNativeImageDialog() {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;

        if (@available(macOS 11.0, *)) {
            panel.allowedContentTypes = @[UTTypeImage];
        }

        if ([panel runModal] == NSModalResponseOK) {
            NSURL *url = [[panel URLs] firstObject];
            return std::string([[url path] UTF8String]);
        }
    }
    return "";
}

std::string SaveNativeFileDialog(const char* defaultFilename) {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];
        panel.nameFieldStringValue = [NSString stringWithUTF8String:defaultFilename];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL *url = [panel URL];
            return std::string([[url path] UTF8String]);
        }
    }
    return "";
}
