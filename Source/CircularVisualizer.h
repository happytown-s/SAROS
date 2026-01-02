#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"

class CircularVisualizer : public juce::Component, public juce::Timer
{
public:
    CircularVisualizer()
        : forwardFFT(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        setOpaque(false); 
        startTimerHz(60);
        setInterceptsMouseClicks(true, true); // マウス操作を確実に受け取る
        
        // Initialize particles
        for (int i = 0; i < numParticles; ++i)
            resetParticle(i);
    }
    

    
    // デバッグ用直線波形表示のオン/オフ
    bool showLinearDebug = false;

    void pushBuffer(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() > 0)
        {
            auto* channelData = buffer.getReadPointer(0);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                pushSampleIntoFifo(channelData[i]);
        }
    }

    // 波形データを追加（履歴として管理）
    // trackLengthSamples: このトラックの録音長
    // masterLengthSamples: 現在のマスターのループ長（1周期の長さ）
    // recordStartGlobal: 録音開始時のグローバル絶対位置
    // masterStartGlobal: マスターのループ開始時のグローバル絶対位置
    void addWaveform(int trackId, const juce::AudioBuffer<float>& buffer, 
                     int trackLengthSamples, int masterLengthSamples, 
                     int recordStartGlobal = 0, int masterStartGlobal = 0)
    {
        // 実際のバッファサイズを使用（渡されたtrackLengthSamplesと異なる可能性あり）
        const int actualBufferSize = buffer.getNumSamples();
        // 描画に使用するサンプル数：バッファサイズとtrackLengthSamplesの小さい方
        const int numSamples = juce::jmin(actualBufferSize, trackLengthSamples);
        if (numSamples == 0 || masterLengthSamples == 0) return;

        const auto* data = buffer.getReadPointer(0);
        const int points = 1024; 
        
        // マスターループに対する比率
        double loopRatio = 0.0;
        if (masterLengthSamples > 0)
            loopRatio = (double)trackLengthSamples / (double)masterLengthSamples;
        
        // マスターとほぼ同じ長さなら、誤差を許容して 1.0 に丸める
        if (loopRatio > 0.95 && loopRatio < 1.05) loopRatio = 1.0;

        // 開始位置のオフセット計算
        // recordStartGlobal と masterStartGlobal が同じなら startAngleRatio = 0
        long offsetFromMasterStart = (long)recordStartGlobal - (long)masterStartGlobal;
        double startAngleRatio = 0.0;
        
        if (masterLengthSamples > 0 && offsetFromMasterStart > 0)
        {
            int relativeStartSample = (int)(offsetFromMasterStart % masterLengthSamples);
            startAngleRatio = (double)relativeStartSample / (double)masterLengthSamples;
        }

        // 🔍 DEBUG LOGGING (バッファサイズ確認追加)
        DBG("🌊 AddWaveform T" << trackId 
            << " | BufferSize: " << actualBufferSize
            << " | TrackLen: " << trackLengthSamples 
            << " | MasterLen: " << masterLengthSamples 
            << " | loopRatio: " << loopRatio
            << " | StartAngleRatio: " << startAngleRatio);

        juce::Path newPath;
        const float maxAmpWidth = 0.3f;

        // ポイント間の正確なサンプル数ステップ（浮動小数点）
        // ★ numSamples (実際読み取る範囲) を基準にする
        double sampleStep = (double)numSamples / (double)points;
        // マニュアルオフセット: -π/2 で12時開始
        // cos/sinでは-π/2 = (0, -1) = 12時
        double manualOffset = -juce::MathConstants<double>::halfPi;

        for (int i = 0; i <= points; ++i)
        {
            float rms = 0.0f;
             // 浮動小数点ステップで開始位置を決定
            double startSampleRaw = i * sampleStep;
            int startSample = (int)startSampleRaw;
            
            // 平均化する範囲も正確に計算 (最低1サンプル)
            int samplesToAverage = (int)sampleStep;
            if (samplesToAverage < 1) samplesToAverage = 1;

            for (int j = 0; j < samplesToAverage; ++j)
            {
                if (startSample + j < numSamples)
                    rms += std::abs(data[startSample + j]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);

            // 進行度: i / points (直線波形と同じ計算)
            // ★ 直線波形は i / linearPoints で位置を決定している
            double progressRaw = (double)i / (double)points;
            
            // 角度計算: startAngle + (progressRaw * loopRatio)
            double currentAngleRatio = startAngleRatio + (progressRaw * loopRatio);
            
            // オフセット適用
            double angleVal = juce::MathConstants<double>::twoPi * currentAngleRatio + manualOffset;
            float angle = (float)angleVal;
            
            float rInner = juce::jmax(0.1f, 1.0f - (rms * maxAmpWidth));
            float xIn = rInner * std::cos(angle);
            float yIn = rInner * std::sin(angle);
            
             if (i == 0) newPath.startNewSubPath(xIn, yIn);
             else        newPath.lineTo(xIn, yIn);
        }

        // 外側の点を逆順に追加
        for (int i = points; i >= 0; --i)
        {
            // 同じロジックで再計算
            double startSampleRaw = i * sampleStep;
            int startSample = (int)startSampleRaw;
            int samplesToAverage = (int)sampleStep;
            if (samplesToAverage < 1) samplesToAverage = 1;

            float rms = 0.0f;
            for (int j = 0; j < samplesToAverage; ++j)
            {
                if (startSample + j < numSamples)
                    rms += std::abs(data[startSample + j]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);

            // ★ 同様に i / points で計算
            double progressRaw = (double)i / (double)points;
            
            double currentAngleRatio = startAngleRatio + (progressRaw * loopRatio);
            
            double angleVal = juce::MathConstants<double>::twoPi * currentAngleRatio + manualOffset;
            float angle = (float)angleVal;
            
            float rOuter = 1.0f + (rms * maxAmpWidth);
            float xOut = rOuter * std::cos(angle);
            float yOut = rOuter * std::sin(angle);
            
            newPath.lineTo(xOut, yOut);
        }
        
        newPath.closeSubPath();

        // 履歴に追加
        WaveformPath wp;
        wp.path = newPath;
        wp.trackId = trackId;
        
        // 8色のネオンカラー
        switch ((trackId - 1) % 8) {
            case 0: wp.colour = ThemeColours::NeonCyan; break;      // シアン
            case 1: wp.colour = ThemeColours::NeonMagenta; break;   // マゼンタ
            case 2: wp.colour = juce::Colour::fromRGB(255, 165, 0); break;   // ネオンオレンジ
            case 3: wp.colour = juce::Colour::fromRGB(57, 255, 20); break;   // ネオングリーン
            case 4: wp.colour = juce::Colour::fromRGB(255, 255, 0); break;   // ネオンイエロー
            case 5: wp.colour = juce::Colour::fromRGB(77, 77, 255); break;   // エレクトリックブルー
            case 6: wp.colour = juce::Colour::fromRGB(191, 0, 255); break;   // ネオンパープル
            case 7: wp.colour = juce::Colour::fromRGB(255, 20, 147); break;  // ネオンピンク
            default: wp.colour = ThemeColours::NeonCyan; break;
        }

        // 既存の同トラックIDの波形があれば削除（重複防止）
        waveformPaths.erase(std::remove_if(waveformPaths.begin(), waveformPaths.end(),
            [trackId](const WaveformPath& w) { return w.trackId == trackId; }), waveformPaths.end());

        waveformPaths.insert(waveformPaths.begin(), wp);
        if (waveformPaths.size() > 8) waveformPaths.resize(8);  // 8トラック分表示
        
        // デバッグ用：リニア波形データを保存
        LinearWaveformData lwd;
        lwd.trackId = trackId;
        lwd.colour = wp.colour;
        lwd.lengthSamples = trackLengthSamples;
        // サンプリング（表示用に間引き）
        const int linearPoints = 512;
        lwd.samples.resize(linearPoints);
        int samplesPerLinearPoint = trackLengthSamples / linearPoints;
        if (samplesPerLinearPoint < 1) samplesPerLinearPoint = 1;
        for (int i = 0; i < linearPoints; ++i)
        {
            float rms = 0.0f;
            int startSample = i * samplesPerLinearPoint;
            for (int j = 0; j < samplesPerLinearPoint && startSample + j < numSamples; ++j)
            {
                rms += std::abs(data[startSample + j]);
            }
            rms /= (float)samplesPerLinearPoint;
            lwd.samples[i] = rms;
        }
        
        // 直線波形も重複防止
        linearWaveforms.erase(std::remove_if(linearWaveforms.begin(), linearWaveforms.end(),
            [trackId](const LinearWaveformData& l) { return l.trackId == trackId; }), linearWaveforms.end());
            
        linearWaveforms.insert(linearWaveforms.begin(), lwd);
        if (linearWaveforms.size() > 8) linearWaveforms.resize(8);
        
        repaint();
    }

    void setPlayHeadPosition(float normalizedPos)
    {
        currentPlayHeadPos = normalizedPos;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // ★ 正方形領域を強制して楕円歪みを防止
        float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto squareArea = bounds.withSizeKeepingCentre(side, side);
        auto centre = squareArea.getCentre();
        auto radius = side * 0.35f;
        if (radius <= 0) return;

        // --- Visualizer Elements (Overlay only) ---
        
        // Background circle
        g.setColour(ThemeColours::MetalGray.withAlpha(0.1f));
        g.fillEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f));

        // --- 1. Particle Field (Stars) ---
        // 画面全体に描画するため、大きな半径を渡す
        float maxParticleDist = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.8f;
        drawParticles(g, centre, maxParticleDist);

        // --- 2. Pulsating Core ---
        float bassLevel = juce::jlimit(0.0f, 1.0f, scopeData[0] * 0.5f + scopeData[1] * 0.3f + scopeData[2] * 0.2f);
        float coreRadius = radius * (0.15f + bassLevel * 0.15f);
        
        float coreAlpha = juce::jlimit(0.0f, 1.0f, 0.6f * (0.5f + bassLevel));
        juce::ColourGradient coreGrad(ThemeColours::NeonCyan.withAlpha(coreAlpha), centre.x, centre.y,
                                     ThemeColours::NeonCyan.withAlpha(0.0f), centre.x + coreRadius, centre.y + coreRadius, true);
        g.setGradientFill(coreGrad);
        g.fillEllipse(centre.x - coreRadius, centre.y - coreRadius, coreRadius * 2.0f, coreRadius * 2.0f);
        
        // Core center light
        float centerAlpha = juce::jlimit(0.0f, 1.0f, 0.4f * (0.3f + bassLevel));
        g.setColour(juce::Colours::white.withAlpha(centerAlpha));
        g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);

        g.setColour(ThemeColours::MetalGray.withAlpha(0.3f));
        g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.1f, radius * 2.1f), 1.0f);

        // --- Draw Concentric Waveforms with Glow ---
        // --- Draw Concentric Waveforms with Glow ---
        // 新しい（i=0）ほど内側（サイズ1.0）、古い（i>0）ほど外側（サイズ>1.0）
        // 大きい方（古い方）から先に描画しないと、内側が隠れてしまうため逆順でループ
        for (int i = (int)waveformPaths.size() - 1; i >= 0; --i)
        {
            const auto& wp = waveformPaths[i];
            
            // i=0 (最新) -> offset 0.0 -> scale 1.0
            // i=1 (古い) -> offset 0.40 -> scale 1.40
            float layerOffset = (float)i * 0.40f;
            float scaleLayer = 1.0f + layerOffset;
            
            // ズーム適用: zoomScaleで全体が拡大（内側に潜る動き）
            float zoomedScale = scaleLayer * zoomScale;
            
            // 画面外に大きくなりすぎたら描画スキップ（適当な上限）
            if (zoomedScale > 5.0f) continue;

            // 出現アニメーション適用: newest spawn starts from center (0.0) -> expands to 1.0
            float finalScale = radius * zoomedScale * wp.spawnProgress;
            
            // アルファ値: 古いほど（外側ほど）薄くするフェードアウト
            // i=0 -> 0.9, i=1 -> 0.8...
            float baseAlpha = (0.9f - layerOffset * 0.5f) * wp.spawnProgress;
            if (baseAlpha < 0.0f) baseAlpha = 0.0f;
            
            auto transform = juce::AffineTransform::scale(finalScale, finalScale)
                                                   .translated(centre.x, centre.y);
            
            juce::Path p = wp.path;
            p.applyTransform(transform);
            
            // Outer glow layers (luminous effect)
            for (int glow = 4; glow >= 1; --glow)
            {
                float glowAlpha = baseAlpha * 0.2f / (float)glow;
                g.setColour(wp.colour.withAlpha(juce::jlimit(0.05f, 0.45f, glowAlpha)));
                g.strokePath(p, juce::PathStrokeType(glow * 4.0f));
            }
            
            // Main fill
            g.setColour(wp.colour.withAlpha(juce::jlimit(0.2f, 0.75f, baseAlpha)));
            g.fillPath(p);
            
            // Inner bright core stroke
            g.setColour(wp.colour.brighter(0.6f).withAlpha(juce::jlimit(0.5f, 1.0f, baseAlpha + 0.35f)));
            g.strokePath(p, juce::PathStrokeType(1.0f));
            
            // Neon edge (extra bright)
            g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.1f, 0.6f, baseAlpha * 0.7f)));
            g.strokePath(p, juce::PathStrokeType(0.3f));
        }
        
        // --- Draw Playhead ---
        if (currentPlayHeadPos >= 0.0f)
        {
            // ★ プレイヘッドはオフセットなし（以前の状態に戻す）
            float manualOffset = 0.0f;
            float angle = (currentPlayHeadPos * juce::MathConstants<float>::twoPi) + manualOffset;
            
            // プレイヘッドライン (レーダーのように中心から外へ)
            auto innerPos = centre.getPointOnCircumference(radius * 0.1f, angle);
            auto outerPos = centre.getPointOnCircumference(radius * 1.1f, angle);
            
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), innerPos.x, innerPos.y,
                                                   juce::Colours::white.withAlpha(0.8f), outerPos.x, outerPos.y, false));
            g.drawLine(innerPos.x, innerPos.y, outerPos.x, outerPos.y, 2.0f);

            auto headPos = centre.getPointOnCircumference(radius, angle);
            g.setColour(juce::Colours::white);
            g.fillEllipse(headPos.x - 3.0f, headPos.y - 3.0f, 6.0f, 6.0f);
        }


        // Draw spinning accent rings
        float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
        
        // Secondary data rings
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.15f));
        drawRotatingRing(g, centre, radius * 1.05f, time, 0.4f);
        g.setColour(ThemeColours::NeonMagenta.withAlpha(0.1f));
        drawRotatingRing(g, centre, radius * 1.1f, -time * 0.7f, 0.3f);
        
        // Dynamic Segmented Ring
        drawSegmentedRing(g, centre, radius * 0.98f, time * 0.5f);
        
        // Outer ring
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.4f));
        g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f), 1.5f);
        
        // --- Draw Circular Spectrum (Outer Audio Visualizer) ---
        const float spectrumRadius = radius * 1.2f;
        const float maxBarHeight = radius * 0.25f;
        const int numBars = scopeSize / 2; // Use half the scope data for smoother look
        
        for (int i = 0; i < numBars; ++i)
        {
            // 3時(0度)開始
            float angle = (float)i / (float)numBars * juce::MathConstants<float>::twoPi;
            float level = scopeData[i * 2]; // Sample every other data point
            float barHeight = level * maxBarHeight;
            
            if (barHeight < 1.0f) continue; // Skip very small bars
            
            auto innerPoint = centre.getPointOnCircumference(spectrumRadius, angle);
            auto outerPoint = centre.getPointOnCircumference(spectrumRadius + barHeight, angle);
            
            // Color gradient from cyan to magenta based on position
            float hue = 0.5f + (float)i / (float)numBars * 0.3f; // Cyan to purple range
            auto barColor = juce::Colour::fromHSV(hue, 0.8f, 0.9f, juce::jlimit(0.3f, 0.9f, level + 0.3f));

            // Gradient from base color to bright tip
            juce::ColourGradient barGrad(barColor.withAlpha(0.4f), innerPoint.x, innerPoint.y,
                                         barColor.brighter(0.8f).withAlpha(0.9f), outerPoint.x, outerPoint.y, false);
            g.setGradientFill(barGrad);
            g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, 2.5f);
            
            // Small bright tip point
            g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, level * 0.8f)));
            g.fillEllipse(outerPoint.x - 1.5f, outerPoint.y - 1.5f, 3.0f, 3.0f);
        }
        
        // ========================================
        // 🔍 DEBUG: Linear Waveform View (Right Side)
        // ========================================
        if (showLinearDebug)
        {
        const float linearAreaX = bounds.getWidth() * 0.68f;
        const float linearAreaY = 20.0f;
        const float linearAreaWidth = bounds.getWidth() * 0.30f;
        const float linearAreaHeight = bounds.getHeight() - 40.0f;
        const float trackRowHeight = linearAreaHeight / (float)juce::jmax(1, (int)linearWaveforms.size());
        
        // 背景
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(linearAreaX, linearAreaY, linearAreaWidth, linearAreaHeight, 5.0f);
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.5f));
        g.drawRoundedRectangle(linearAreaX, linearAreaY, linearAreaWidth, linearAreaHeight, 5.0f, 1.0f);
        
        // 各トラックの波形を描画
        for (size_t t = 0; t < linearWaveforms.size(); ++t)
        {
            const auto& lwd = linearWaveforms[t];
            float rowY = linearAreaY + (float)t * trackRowHeight;
            float waveHeight = trackRowHeight * 0.8f;
            float centerY = rowY + trackRowHeight * 0.5f;
            float waveWidth = linearAreaWidth - 10.0f;
            float startX = linearAreaX + 5.0f;
            
            // 波形描画
            juce::Path linearPath;
            for (size_t i = 0; i < lwd.samples.size(); ++i)
            {
                float x = startX + (float)i / (float)lwd.samples.size() * waveWidth;
                float amplitude = lwd.samples[i] * waveHeight * 2.0f;
                float y1 = centerY - amplitude * 0.5f;
                float y2 = centerY + amplitude * 0.5f;
                
                if (i == 0)
                    linearPath.startNewSubPath(x, y1);
                else
                    linearPath.lineTo(x, y1);
            }
            // 折り返し
            for (int i = (int)lwd.samples.size() - 1; i >= 0; --i)
            {
                float x = startX + (float)i / (float)lwd.samples.size() * waveWidth;
                float amplitude = lwd.samples[i] * waveHeight * 2.0f;
                float y2 = centerY + amplitude * 0.5f;
                linearPath.lineTo(x, y2);
            }
            linearPath.closeSubPath();
            
            g.setColour(lwd.colour.withAlpha(0.6f));
            g.fillPath(linearPath);
            g.setColour(lwd.colour);
            g.strokePath(linearPath, juce::PathStrokeType(1.0f));
            
            // トラックID表示
            g.setColour(juce::Colours::white);
            g.drawText("T" + juce::String(lwd.trackId), (int)startX, (int)rowY, 30, 15, juce::Justification::left);
        }
        
        // プレイヘッド（縦線）
        if (currentPlayHeadPos >= 0.0f && !linearWaveforms.empty())
        {
            float playheadX = linearAreaX + 5.0f + currentPlayHeadPos * (linearAreaWidth - 10.0f);
            g.setColour(juce::Colours::white);
            g.drawLine(playheadX, linearAreaY + 5.0f, playheadX, linearAreaY + linearAreaHeight - 5.0f, 2.0f);
        }
        } // end showLinearDebug
    }


    // 全リセット
    void clear()
    {
        waveformPaths.clear();
        linearWaveforms.clear();
        currentPlayHeadPos = -1.0f;
        juce::zeromem(scopeData, sizeof(scopeData));
        repaint();
    }

    void timerCallback() override
    {
        updateParticles();
        
        // スムーズなズームアニメーション (0.15 -> 0.05 ゆっくり)
        zoomScale += (targetZoomScale - zoomScale) * 0.05f;
        
        // 波形の出現アニメーション (0.15 -> 0.05 ゆっくり)
        for (auto& wp : waveformPaths)
        {
            if (wp.spawnProgress < 1.0f) {
                wp.spawnProgress += (1.0f - wp.spawnProgress) * 0.05f;
                if (std::abs(1.0f - wp.spawnProgress) < 0.001f) wp.spawnProgress = 1.0f;
            }
        }
        
        repaint(); // Always repaint for animations
        
        if (nextFFTBlockReady)
        {
            drawNextFrameOfSpectrum();
            nextFFTBlockReady = false;
        }
    }

private:
   
    struct WaveformPath
    {
        juce::Path path;
        juce::Colour colour;
        int trackId = 0;
        float spawnProgress = 0.0f; // 0.0 -> 1.0 アニメーション用
    };
    std::vector<WaveformPath> waveformPaths;
    
    // デバッグ用リニア波形データ
    struct LinearWaveformData
    {
        int trackId = 0;
        int lengthSamples = 0;
        juce::Colour colour;
        std::vector<float> samples; // RMS値の配列
    };
    std::vector<LinearWaveformData> linearWaveforms;
    
    float currentPlayHeadPos = -1.0f;
    
    // ズーム機能用
    // ズーム機能用
    float zoomScale = 1.0f;           // 1.0 = 通常、>1.0 = ズームイン
    float targetZoomScale = 1.0f;     // スムーズなアニメーション用
    bool isDragging = false;
    juce::Point<float> lastDragPos;
    float dragVelocityRemaining = 0.0f; 
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging)
        {
            isDragging = true;
            lastDragPos = e.position;
            return;
        }
        
        // 垂直ドラッグでズーム制御（上にドラッグ = ズームイン）
        float deltaY = lastDragPos.y - e.position.y;
        targetZoomScale += deltaY * 0.01f;
        // 0.2倍まで縮小可能にして、外側の波形も見えるようにする
        targetZoomScale = juce::jlimit(0.2f, 5.0f, targetZoomScale);
        
        // アニメーション制御: ドラッグ量に応じて加速/逆回転
        // 上ドラッグ(deltaY > 0) -> 拡大 -> 拡散(反対方向) -> 負の力
        // 下ドラッグ(deltaY < 0) -> 縮小 -> 収束加速(通常方向) -> 正の力
        // 係数は感度調整
        // 変化を分かりやすくするために係数を大幅アップ (5.0 -> 30.0)
        dragVelocityRemaining = -deltaY * 30.0f; 
        
        lastDragPos = e.position;
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        // ダブルクリックでリセット
        targetZoomScale = 1.0f;
    }
    
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        // マウスホイールでもズーム
        targetZoomScale += wheel.deltaY * 0.5f;
        targetZoomScale = juce::jlimit(0.2f, 5.0f, targetZoomScale);
        
        // ホイール操作もアニメーション連動 (感度高めに)
        dragVelocityRemaining = -wheel.deltaY * 60.0f;
    }

    void drawRotatingRing(juce::Graphics& g, juce::Point<float> centre, float radius, float rotation, float arcLength)
    {
        juce::Path ring;
        ring.addCentredArc(centre.x, centre.y, radius, radius, rotation, 0.0f, juce::MathConstants<float>::twoPi * arcLength, true);
        g.strokePath(ring, juce::PathStrokeType(1.5f));
    }

    void drawSegmentedRing(juce::Graphics& g, juce::Point<float> centre, float radius, float rotation)
    {
        const int segments = 12;
        const float gap = 0.1f;
        const float segmentLen = (juce::MathConstants<float>::twoPi / (float)segments) * (1.0f - gap);
        
        for (int i = 0; i < segments; ++i)
        {
            float startAngle = rotation + (float)i * (juce::MathConstants<float>::twoPi / (float)segments);
            juce::Path seg;
            seg.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, startAngle + segmentLen, true);
            g.setColour(ThemeColours::NeonCyan.withAlpha(i % 3 == 0 ? 0.4f : 0.15f));
            g.strokePath(seg, juce::PathStrokeType(1.0f));
            
            // Ticks
            auto tickPos = centre.getPointOnCircumference(radius, startAngle);
            auto tickEnd = centre.getPointOnCircumference(radius + 3.0f, startAngle);
            g.drawLine(tickPos.x, tickPos.y, tickEnd.x, tickEnd.y, 0.5f);
        }
    }

    struct Particle
    {
        float x, y;
        float vx, vy;
        float alpha;
        float size;
        float life;
    };
    static constexpr int numParticles = 120; // 画面全体にするので数を増やす (40 -> 120)
    Particle particles[numParticles];

    void resetParticle(int i)
    {
        // 外周からスタートして中心に向かう
        // 画面全体に広げるため、コンポーネントのサイズを使用
        float radiusMax = (float)juce::jmax(getWidth(), getHeight()) * 0.7f;
        if (radiusMax < 100.0f) radiusMax = 400.0f; // 初期化時などサイズ未定時のフォールバック

        float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        
        float startRadius = 0.0f;
        
        // 拡散モード(逆再生)のときは、中心付近から湧き出るようにする
        if (dragVelocityRemaining < -1.0f) // 閾値
        {
             startRadius = juce::Random::getSystemRandom().nextFloat() * 50.0f;
        }
        else
        {
            // 通常モード: 外周から湧き出る
            startRadius = radiusMax * (0.5f + juce::Random::getSystemRandom().nextFloat() * 0.5f); 
        }
        
        particles[i].x = std::cos(angle) * startRadius;
        particles[i].y = std::sin(angle) * startRadius;
        particles[i].vx = 0; // 速度は updateParticles で計算
        particles[i].vy = 0;
        particles[i].alpha = 0.3f + juce::Random::getSystemRandom().nextFloat() * 0.5f;
        particles[i].size = 1.0f + juce::Random::getSystemRandom().nextFloat() * 2.5f; // 少しサイズばらつき大きく
        particles[i].life = 1.0f;
    }

    void updateParticles()
    {
        float bassLevel = scopeData[0] * 0.5f + scopeData[1] * 0.5f;
        float attractStrength = 0.3f + bassLevel * 0.5f; // 低音に反応して吸引力が強くなる
        
        // ベースの力 (通常時ゆっくり) + ドラッグによる追加力
        // dragVelocityRemainingが正なら収束加速、負なら拡散
        float currentAdditionalForce = dragVelocityRemaining * 0.05f;
        
        // ループ外でキャッシュ（パフォーマンス最適化）
        bool isDiffusing = (dragVelocityRemaining < -0.1f);
        float screenMax = (float)juce::jmax(getWidth(), getHeight());
        float outOfBoundsRadius = isDiffusing ? screenMax * 3.0f : screenMax * 1.5f;
        
        for (int i = 0; i < numParticles; ++i)
        {
            float dist = std::sqrt(particles[i].x * particles[i].x + particles[i].y * particles[i].y);
            
            if (dist > 1.0f)
            {
                // 中心に向かうベクトルを計算
                float dirX = -particles[i].x / dist;
                float dirY = -particles[i].y / dist;
                
                // 力の合成: 通常引力(正) + 追加力(正or負)
                // 追加力が強烈な負の場合、全体として負(拡散)になる
                float totalForce = (attractStrength * 0.015f) + currentAdditionalForce;
                
                particles[i].vx += dirX * totalForce;
                particles[i].vy += dirY * totalForce;
                
                // 速度制限 (描画飛び防止)
                float speedSq = particles[i].vx * particles[i].vx + particles[i].vy * particles[i].vy;
                if (speedSq > 2500.0f) { // Max speed 50.0
                    float scale = 50.0f / std::sqrt(speedSq);
                    particles[i].vx *= scale;
                    particles[i].vy *= scale;
                }

                // 速度を適用
                particles[i].x += particles[i].vx;
                particles[i].y += particles[i].vy;
                
                // 減衰（慣性を残しつつゆっくり）
                particles[i].vx *= 0.99f;
                particles[i].vy *= 0.99f;
            }
            
            if (!isDiffusing)
            {
                // 収束モード: 外に向かっているか、中心付近ならリセット
                bool movingAway = (particles[i].x * particles[i].vx + particles[i].y * particles[i].vy) > 0;
                if (movingAway || dist < 20.0f)
                {
                    resetParticle(i);
                    continue; 
                }
            }
            else
            {
                // 拡散モード: フェードアウト
                particles[i].life -= 0.005f;
                particles[i].alpha *= 0.995f;
            }
            
            // 画面外 or 寿命尽きたらリセット
            if (particles[i].life <= 0 || (!isDiffusing && dist < 2.0f) || dist > outOfBoundsRadius)
                resetParticle(i);
        }
        
        // ドラッグ力の減衰 (慣性)
        dragVelocityRemaining *= 0.92f;
        if (std::abs(dragVelocityRemaining) < 0.001f) dragVelocityRemaining = 0.0f;
    }

    void drawParticles(juce::Graphics& g, juce::Point<float> centre, float maxRadius)
    {
        // 0 除算防止
        if (maxRadius < 1.0f) maxRadius = 400.0f;
        
        for (int i = 0; i < numParticles; ++i)
        {
            float px = centre.x + particles[i].x;
            float py = centre.y + particles[i].y;
            float dist = std::sqrt(particles[i].x * particles[i].x + particles[i].y * particles[i].y);
            
            if (dist > maxRadius * 1.5f) continue;
            
            // 中心に近いほど明るく、光の収束を表現
            float proximityBonus = juce::jlimit(0.0f, 1.0f, 1.0f - (dist / maxRadius));
            float alpha = juce::jlimit(0.0f, 1.0f, particles[i].alpha * particles[i].life * (0.3f + proximityBonus * 0.7f));
            
            // パーティクル本体
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.fillEllipse(px - particles[i].size*0.5f, py - particles[i].size*0.5f, particles[i].size, particles[i].size);
            
            // グロウ（中心に近いほど強い）
            float glowAlpha = juce::jlimit(0.0f, 1.0f, alpha * 0.4f * (0.5f + proximityBonus * 0.5f));
            g.setColour(ThemeColours::NeonCyan.withAlpha(glowAlpha));
            g.fillEllipse(px - particles[i].size, py - particles[i].size, particles[i].size*2.0f, particles[i].size*2.0f);
        }
    }
    void pushSampleIntoFifo(float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                juce::zeromem(fftData, sizeof(fftData));
                std::memcpy(fftData, fifo, sizeof(fifo));
                nextFFTBlockReady = true;
            }
            fifoIndex = 0;
        }
        fifo[fifoIndex++] = sample;
    }

    void drawNextFrameOfSpectrum()
    {
        window.multiplyWithWindowingTable(fftData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);

        auto mindB = -100.0f;
        auto maxdB = 0.0f;
        const float decayRate = 0.85f; // Slow decay (higher = slower)

        for (int i = 0; i < scopeSize; ++i)
        {
            auto skewedProportionX = 1.0f - std::exp(std::log(1.0f - (float)i / (float)scopeSize) * 0.2f);
            auto fftDataIndex = juce::jlimit(0, fftSize / 2, (int)(skewedProportionX * (float)fftSize / 2));
            auto newLevel = juce::jmap(juce::Decibels::gainToDecibels(fftData[fftDataIndex]) - juce::Decibels::gainToDecibels((float)fftSize), mindB, maxdB, 0.0f, 1.0f);
            newLevel = juce::jlimit(0.0f, 1.0f, newLevel);

            // Apply decay: only decrease slowly, increase immediately
            if (newLevel > scopeData[i])
                scopeData[i] = newLevel;
            else
                scopeData[i] = scopeData[i] * decayRate + newLevel * (1.0f - decayRate);
        }
    }

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = 256;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[fftSize];
    float fftData[fftSize * 2];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;
    float scopeData[scopeSize];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularVisualizer)
};
