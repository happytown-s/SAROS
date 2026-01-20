/*
  ==============================================================================

    SystemAudioCapturer.mm
    Created: 19 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/


#include "SystemAudioCapturer.h"
#include <ScreenCaptureKit/ScreenCaptureKit.h>
#include <CoreMedia/CoreMedia.h>
#include <AppKit/AppKit.h>

//==============================================================================
// Objective-C Delegate Interface
//==============================================================================

API_AVAILABLE(macos(13.0))
@interface AudioCaptureDelegate : NSObject <SCStreamOutput>
{
@public
    juce::AbstractFifo* fifo;
    juce::AudioBuffer<float>* ringBuffer;
}
- (instancetype)initWithFifo:(juce::AbstractFifo*)fifo ringBuffer:(juce::AudioBuffer<float>*)ringBuffer;
@end

@implementation AudioCaptureDelegate

- (instancetype)initWithFifo:(juce::AbstractFifo*)f ringBuffer:(juce::AudioBuffer<float>*)b
{
    self = [super init];
    if (self)
    {
        fifo = f;
        ringBuffer = b;
    }
    return self;
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
    // Debug: Log when first audio buffer is received
    static int callbackCount = 0;
    if (callbackCount < 5)
    {
        DBG("📦 SCK Callback #" << callbackCount << " Type: " << (int)type);
        callbackCount++;
    }
    
    if (type != SCStreamOutputTypeAudio) return;
    
    // オーディオフォーマット情報の取得
    CMAudioFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(formatDesc);
    
    if (!asbd || asbd->mFormatID != kAudioFormatLinearPCM) return;
    
    // サンプル数
    CMItemCount numSamples = CMSampleBufferGetNumSamples(sampleBuffer);
    if (numSamples == 0) return;
    
    // データバッファの取得
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer) return;
    
    // データポインタの取得
    size_t lengthAtOffset;
    size_t totalLength;
    char *dataPointer = nullptr;
    if (CMBlockBufferGetDataPointer(blockBuffer, 0, &lengthAtOffset, &totalLength, &dataPointer) != kCMBlockBufferNoErr)
    {
        return;
    }

    // SCKのオーディオは通常 Float32 or Int16
    bool isFloat = (asbd->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
    int channels = asbd->mChannelsPerFrame;
    
    // 簡易実装: Float32 のみ対応
    if (isFloat && asbd->mBitsPerChannel == 32)
    {
        // Debug: Log first few audio buffers with actual data and format info
        static int audioLogCount = 0;
        if (audioLogCount < 5)
        {
            float* src = (float*)dataPointer;
            float maxSample = 0.0f;
            for (int i = 0; i < std::min((int)numSamples, 100); ++i)
            {
                maxSample = std::max(maxSample, std::abs(src[i * channels]));
            }
            DBG("🔊 Audio Buffer #" << audioLogCount << " Samples=" << numSamples 
                << " Ch=" << channels << " Rate=" << (int)asbd->mSampleRate 
                << " MaxLevel=" << maxSample);
            audioLogCount++;
        }
        
        // リングバッファの空き容量確認
        int freeSpace = fifo->getFreeSpace();
        if (freeSpace < numSamples) return; // オーバーフロー時はドロップ
        
        int start1, size1, start2, size2;
        fifo->prepareToWrite((int)numSamples, start1, size1, start2, size2);
        
        float* src = (float*)dataPointer;
        
        // Lambda captures by reference naturally in C++.
        auto writeToRing = [&](int numToWrite, int offset) {
            for (int i = 0; i < numToWrite; ++i)
            {
                // ソースの各チャンネルをリングバッファの各チャンネルにコピー
                for (int ch = 0; ch < ringBuffer->getNumChannels(); ++ch)
                {
                    float sample = 0.0f;
                    if (ch < channels)
                    {
                        // Interleaved: sample[frame * channels + ch]
                        sample = src[i * channels + ch];
                    }
                    ringBuffer->setSample(ch, offset + i, sample);
                }
            }
            src += numToWrite * channels;
        };
        
        if (size1 > 0) writeToRing(size1, start1);
        if (size2 > 0) writeToRing(size2, start2);
        
        fifo->finishedWrite((int)numSamples);
    }
    else
    {
        // DBG("⚠️ SCK: Unsupported Audio Format: " << (isFloat ? "Float" : "Int") << " " << asbd->mBitsPerChannel << "bit");
    }
}
@end

//==============================================================================
// Internal Pimpl Class (C++)
//==============================================================================
struct SystemAudioCapturer::Pimpl
{
    // リングバッファ: 100ms 程度で十分
    // 大きすぎるとレイテンシが増える
    Pimpl() : captureFifo(4800), ringBuffer(2, 4800)
    {
        if (@available(macOS 13.0, *)) {
            delegate = [[AudioCaptureDelegate alloc] initWithFifo:&captureFifo ringBuffer:&ringBuffer];
        }
    }

    void prepare(double sr, int bufferSize)
    {
        targetSampleRate = sr;
        // リングバッファを50ms分に設定（レイテンシ削減）
        int ringSize = (int)(sr * 0.05);
        captureFifo.setTotalSize(ringSize);
        captureFifo.reset();
        ringBuffer.setSize(2, ringSize);
        ringBuffer.clear();
        DBG("🔧 SCK Prepared: sampleRate=" << sr << " ringSize=" << ringSize);
    }

    ~Pimpl()
    {
        stop();
        if (delegate) {
            [delegate release];
            delegate = nil;
        }
    }
    
    void start()
    {
        if (isCapturing) return;

        // 保存されたPIDsをコピー（非同期ブロックで使用）
        std::vector<int> pidsCopy;
        {
            std::lock_guard<std::mutex> lock(pidsMutex);
            pidsCopy = selectedPIDs;
        }

        if (@available(macOS 13.0, *))
        {
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
                if (error)
                {
                    DBG("SCShareableContent error: " << error.localizedDescription.UTF8String);
                    return;
                }

                SCDisplay *display = [content.displays firstObject];
                if (!display) {
                    DBG("❌ SCShareableContent: No display found!");
                    return;
                }

                DBG("✅ Found Display: " << (int)display.width << "x" << (int)display.height);

                // 保存されたPIDsに基づいてフィルターを作成
                SCContentFilter *filter = nil;
                if (!pidsCopy.empty())
                {
                    // 選択されたアプリのみキャプチャ
                    NSMutableArray *appsToInclude = [NSMutableArray array];
                    for (SCRunningApplication *app in content.applications)
                    {
                        for (int pid : pidsCopy)
                        {
                            if (app.processID == pid)
                            {
                                [appsToInclude addObject:app];
                                break;
                            }
                        }
                    }

                    DBG("🎯 SCK Start: Including " << (int)[appsToInclude count] << " apps");
                    filter = [[SCContentFilter alloc] initWithDisplay:display
                                                 includingApplications:appsToInclude
                                                      exceptingWindows:@[]];
                }
                else
                {
                    // PIDs未設定の場合は全アプリをキャプチャ（自分自身除く）
                    DBG("🎯 SCK Start: Capturing all apps (no filter set)");
                    filter = [[SCContentFilter alloc] initWithDisplay:display
                                                 excludingApplications:@[]
                                                      exceptingWindows:@[]];
                }

                SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
                config.capturesAudio = YES;

                // デバイスのサンプルレートに合わせる（ピッチずれ防止）
                config.sampleRate = (NSInteger)targetSampleRate;
                config.channelCount = 2;
                config.excludesCurrentProcessAudio = YES;

                DBG("🔧 SCK Config: sampleRate=" << (int)config.sampleRate);

                stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:nil];
                
                NSError *addOutputError = nil;
                // Use a dedicated serial queue for audio capture
                dispatch_queue_t audioQueue = dispatch_queue_create("com.babyleaf.saros.audiocapture", DISPATCH_QUEUE_SERIAL);
                
                if (![stream addStreamOutput:(id<SCStreamOutput>)delegate type:SCStreamOutputTypeAudio sampleHandlerQueue:audioQueue error:&addOutputError])
                {
                     DBG("❌ Failed to add stream output: " << addOutputError.localizedDescription.UTF8String);
                     return;
                }

                [stream startCaptureWithCompletionHandler:^(NSError *startError) {
                    if (startError) {
                        DBG("❌ Start capture error: " << startError.localizedDescription.UTF8String);
                    } else {
                        DBG("🔊 ScreenCaptureKit Audio Started Successfully");
                        isCapturing = true;
                    }
                }];
            }];
        }
        else
        {
            DBG("⚠️ SystemAudioCapturer requires macOS 13.0 or later.");
        }
    }

    void stop()
    {
        if (@available(macOS 13.0, *)) {
            if (stream && isCapturing)
            {
                dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
                
                [stream stopCaptureWithCompletionHandler:^(NSError *error) {
                    isCapturing = false;
                    dispatch_semaphore_signal(semaphore);
                }];
                
                // Wait for stop to complete (max 1 second)
                dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC));
            }
            stream = nil;
        }
    }

    int getNextAudioBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (!isCapturing) return 0;

        int numReady = captureFifo.getNumReady();

        // 十分なサンプルがない場合は読まない
        if (numReady < numSamples) return 0;

        // レイテンシ削減: バッファが溜まりすぎたら古いサンプルを捨てる
        // 目標: 最大2ブロック分（約10-20ms）のバッファリング
        int maxBuffer = numSamples * 2;
        if (numReady > maxBuffer)
        {
            int toDiscard = numReady - maxBuffer;
            int start1, size1, start2, size2;
            captureFifo.prepareToRead(toDiscard, start1, size1, start2, size2);
            captureFifo.finishedRead(size1 + size2);
            // DBG("⏩ SCK: Discarded " << toDiscard << " samples to reduce latency");
        }

        // フルブロック読み取り
        int samplesToRead = numSamples;
        
        int start1, size1, start2, size2;
        captureFifo.prepareToRead(samplesToRead, start1, size1, start2, size2);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            int srcCh = ch % ringBuffer.getNumChannels();
            if (size1 > 0)
                buffer.addFrom(ch, startSample, ringBuffer, srcCh, start1, size1);
            if (size2 > 0)
                buffer.addFrom(ch, startSample + size1, ringBuffer, srcCh, start2, size2);
        }
        
        captureFifo.finishedRead(samplesToRead);
        
        // メーター用レベル更新
        float level = buffer.getMagnitude(startSample, samplesToRead);
        float current = currentLevel.load();
        if (level > current)
            currentLevel.store(level);
        else
            currentLevel.store(current * 0.95f + level * 0.05f); // スムーズ減衰
        
        return samplesToRead;
    }

    std::vector<ShareableApp> getAvailableApps()
    {
        std::vector<ShareableApp> apps;
        
        if (@available(macOS 13.0, *))
        {
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            auto* appsPtr = &apps;
            
            DBG("🔍 Requesting SCShareableContent...");
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
                if (!error)
                {
                    DBG("✅ SCShareableContent found " << (int)content.applications.count << " total apps.");
                    int addedCount = 0;
                    for (SCRunningApplication *app in content.applications)
                    {
                        // DBG("  - Checking: " << app.applicationName.UTF8String << " (PID: " << app.processID << ")"); 
                        
                        // Use NSRunningApplication to filter and get bundleURL
                        NSRunningApplication* runningApp = [NSRunningApplication runningApplicationWithProcessIdentifier:app.processID];
                        
                        // Filter: Only regular apps (exclude background agents)
                        if (!runningApp) {
                            // DBG("    -> No NSRunningApplication found, skipped.");
                            continue;
                        }
                        
                        // Filter: Exclude self (current process)
                        if (app.processID == getpid()) {
                            continue;
                        }
                        
                        if (runningApp.activationPolicy != NSApplicationActivationPolicyRegular) {
                            // DBG("    -> Not Regular Policy, skipped.");
                            continue;
                        }

                        ShareableApp sa;
                        sa.processID = (int)app.processID;
                        
                        // Safer string conversion: copy to local, check UTF8String
                        NSString* appNameStr = app.applicationName;
                        if (appNameStr != nil && appNameStr.length > 0)
                        {
                            const char* utf8Ptr = [appNameStr UTF8String];
                            if (utf8Ptr != nullptr)
                            {
                                sa.appName = juce::String::fromUTF8(utf8Ptr);
                            }
                            else
                            {
                                sa.appName = "Unknown App (" + juce::String(app.processID) + ")";
                            }
                        }
                        else
                        {
                            sa.appName = "Unknown App (" + juce::String(app.processID) + ")";
                        }
                        
                        // Use NSRunningApplication to filter and get bundleURL
                        if (runningApp.bundleURL)
                        {
                            if (NSImage* nsImage = [[NSWorkspace sharedWorkspace] iconForFile:runningApp.bundleURL.path])
                            {
                                sa.icon = convertNSImageToJuceImage(nsImage);
                            }
                        }
                        
                        if (sa.icon.isNull())
                        {
                             sa.icon = juce::Image(juce::Image::ARGB, 32, 32, true); 
                        }

                        appsPtr->push_back(sa);
                        addedCount++;
                    }
                    DBG("📝 Added " << addedCount << " apps to list after filtering.");
                }
                else
                {
                    DBG("❌ Failed to get shareable content: " << error.localizedDescription.UTF8String);
                    DBG("❌ Error Code: " << (int)error.code << " Domain: " << error.domain.UTF8String);
                }
                dispatch_semaphore_signal(semaphore);
            }];
            
            dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
        }
        return apps;
    }

    void updateFilter(const std::vector<int>& includedPIDs)
    {
        // PIDs を保存（キャプチャ開始前の設定も保持）
        {
            std::lock_guard<std::mutex> lock(pidsMutex);
            selectedPIDs = includedPIDs;
        }
        DBG("🔧 SCK Filter Updated: " << (int)includedPIDs.size() << " apps selected");

        if (!isCapturing || !stream) return;

        // IMPORTANT: Copy the vector for async block capture
        std::vector<int> pidsCopy = includedPIDs;
        
        if (@available(macOS 13.0, *))
        {
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
                if (error) return;
                
                SCDisplay *display = [content.displays firstObject];
                if (!display) return;
                
                NSMutableArray *appsToInclude = [NSMutableArray array];
                for (SCRunningApplication *app in content.applications)
                {
                    for (int pid : pidsCopy)
                    {
                        if (app.processID == pid)
                        {
                            [appsToInclude addObject:app];
                            break;
                        }
                    }
                }
                
                SCContentFilter *newFilter = nil;
                if ([appsToInclude count] > 0)
                {
                    newFilter = [[SCContentFilter alloc] initWithDisplay:display
                                                   includingApplications:appsToInclude
                                                        exceptingWindows:@[]];
                }
                else
                {
                     newFilter = [[SCContentFilter alloc] initWithDisplay:display
                                                   excludingApplications:@[]
                                                        exceptingWindows:@[]];
                }
                
                [stream updateContentFilter:newFilter completionHandler:^(NSError * _Nullable error) {
                    if (error) {
                        DBG("Failed to update filter: " << error.localizedDescription.UTF8String);
                    } else {
                        DBG("✅ Capture Filter Updated");
                    }
                }];
            }];
        }
    }
    
    juce::Image convertNSImageToJuceImage(NSImage* nsImage)
    {
        if (!nsImage) return {};

        CGImageRef cgImage = [nsImage CGImageForProposedRect:nullptr context:nil hints:nil];
        if (!cgImage) return {};
        
        NSBitmapImageRep* bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        if (!bitmapRep) return {};

        int width = (int)bitmapRep.pixelsWide;
        int height = (int)bitmapRep.pixelsHigh;
        
        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData destData(image, juce::Image::BitmapData::writeOnly);

        unsigned char* srcData = [bitmapRep bitmapData];
        int srcStride = (int)[bitmapRep bytesPerRow];
        int components = (int)[bitmapRep samplesPerPixel];

        for (int y = 0; y < height; ++y)
        {
            unsigned char* destRow = destData.getLinePointer(y);
            unsigned char* srcRow = srcData + y * srcStride;
            
            for (int x = 0; x < width; ++x)
            {
                unsigned char r = srcRow[x * components + 0];
                unsigned char g = srcRow[x * components + 1];
                unsigned char b = srcRow[x * components + 2];
                unsigned char a = (components > 3) ? srcRow[x * components + 3] : 255;
                
                int destIndex = x * destData.pixelStride;
                destRow[destIndex + 0] = b; 
                destRow[destIndex + 1] = g;
                destRow[destIndex + 2] = r;
                destRow[destIndex + 3] = a;
            }
        }
        
        return image;
    }

    SCStream *stream = nil;
    AudioCaptureDelegate *delegate = nil;
    std::atomic<bool> isCapturing { false };
    std::atomic<float> currentLevel { 0.0f };

    double targetSampleRate = 44100.0;
    juce::AbstractFifo captureFifo;
    juce::AudioBuffer<float> ringBuffer;

    // フィルター用: 選択されたPIDを保持（キャプチャ開始時に適用）
    std::vector<int> selectedPIDs;
    std::mutex pidsMutex;
};

//==============================================================================
// Wrapper Implementation
//==============================================================================

SystemAudioCapturer::SystemAudioCapturer()
    : pimpl(std::make_unique<Pimpl>())
{
}

SystemAudioCapturer::~SystemAudioCapturer() = default;

void SystemAudioCapturer::prepare(double sampleRate, int bufferSize)
{
    pimpl->prepare(sampleRate, bufferSize);
}

void SystemAudioCapturer::start()
{
    pimpl->start();
}

void SystemAudioCapturer::stop()
{
    pimpl->stop();
}

bool SystemAudioCapturer::isCapturing() const
{
    return pimpl->isCapturing.load();
}

int SystemAudioCapturer::getNextAudioBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    return pimpl->getNextAudioBlock(buffer, startSample, numSamples);
}

std::vector<ShareableApp> SystemAudioCapturer::getAvailableApps() const
{
    return pimpl->getAvailableApps();
}

void SystemAudioCapturer::updateFilter(const std::vector<int>& includedPIDs)
{
    pimpl->updateFilter(includedPIDs);
}

float SystemAudioCapturer::getCurrentLevel() const
{
    return pimpl->currentLevel.load();
}
