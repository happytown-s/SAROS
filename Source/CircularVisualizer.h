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
        
        if (masterLengthSamples > 0)
        {
            // 正負両方のオフセットを正しく処理
            long relativeStart = offsetFromMasterStart % masterLengthSamples;
            if (relativeStart < 0) relativeStart += masterLengthSamples; // 負の剰余を正に変換
            startAngleRatio = (double)relativeStart / (double)masterLengthSamples;
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
        // マニュアルオフセット: 0.0で3時開始 (プレイヘッドに合わせる)
        double manualOffset = 0.0;

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

        // オリジナルデータを保存（multiplier変更時の再計算用）
        wp.originalBuffer.makeCopyOf(buffer);
        wp.originalTrackLength = trackLengthSamples;
        wp.originalMasterLength = masterLengthSamples;
        wp.originalRecordStart = recordStartGlobal;
        wp.originalMasterStart = masterStartGlobal;
        wp.loopMultiplier = loopRatio; // トラック長/マスター長をmultiplierとして設定

        waveformPaths.insert(waveformPaths.begin(), wp);
        if (waveformPaths.size() > 8) waveformPaths.resize(8);  // 8トラック分表示
        
        // 現在のmaxMultiplierに基づいてパスを再生成（正しいリピート表示のため）
        if (maxMultiplier > 0.0f && !waveformPaths.empty())
        {
            regenerateWaveformPath(waveformPaths.front(), 0, masterLengthSamples);
        }
        
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
    
    // 指定トラックの波形を削除
    void removeWaveform(int trackId)
    {
        waveformPaths.erase(std::remove_if(waveformPaths.begin(), waveformPaths.end(),
            [trackId](const WaveformPath& w) { return w.trackId == trackId; }), waveformPaths.end());
        
        linearWaveforms.erase(std::remove_if(linearWaveforms.begin(), linearWaveforms.end(),
            [trackId](const LinearWaveformData& w) { return w.trackId == trackId; }), linearWaveforms.end());
        
        DBG("🗑 Removed waveform for track " << trackId);
        repaint();
    }
    

    // 指定トラックのloopMultiplierを変更（再生成はしない）
    void setTrackMultiplier(int trackId, float multiplier)
    {
        DBG("🔍 setTrackMultiplier: trackId=" << trackId << " multiplier=" << multiplier);
        for (auto& wp : waveformPaths)
        {
            if (wp.trackId == trackId)
                wp.loopMultiplier = multiplier;
        }
    }
    
    
    // 全トラックの最大倍率（最長トラック）を設定し、全ての波形を再生成
    void setMaxMultiplier(float newMax)
    {
        maxMultiplier = newMax;
        activeMultiplier = newMax;
        
        DBG("🔄 setMaxMultiplier: " << newMax);
        
        for (auto& wp : waveformPaths)
        {
            if (wp.originalBuffer.getNumSamples() > 0)
            {
                // リピート回数 = maxMultiplier / loopMultiplier
                regenerateWaveformPath(wp, 0, wp.originalMasterLength);
            }
        }
        repaint();
    }
    void setPlayHeadPosition(float normalizedPos)
    {
        // ループのラップアラウンドを検出してループカウントを更新
        if (normalizedPos < lastPlayHeadPos - 0.5f)  // 0.9から0.1へ等の大きなジャンプ
        {
            loopCount++;
        }
        lastPlayHeadPos = normalizedPos;
        
        // x2モード時：2マスターループで1周完結するよう累積位置を計算
        if (activeMultiplier > 1.0f)
        {
            float loopOffset = (float)(loopCount % (int)activeMultiplier) / activeMultiplier;
            currentPlayHeadPos = loopOffset + (normalizedPos / activeMultiplier);
        }
        else
        {
            currentPlayHeadPos = normalizedPos;
        }
    }
    
    void resetPlayHead()
    {
        loopCount = 0;
        lastPlayHeadPos = 0.0f;
        currentPlayHeadPos = -1.0f;
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

        // --- 1. Particle Field (White Smoke / Stars) ---
        // 画面全体に描画するため、大きな半径を渡す
        float maxParticleDist = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.8f;
        
        // マスターレベル（全体の音量感）を計算
        // 平均的なエネルギーを使用 - scopeDataは負になる可能性があるのでクランプ
        float masterLevel = 0.0f;
        int levelCount = 0;
        for (int i = 0; i < scopeSize / 2; ++i) {
            masterLevel += std::max(0.0f, scopeData[i]);
            levelCount++;
        }
        if (levelCount > 0) masterLevel /= (float)levelCount;
        masterLevel = juce::jlimit(0.0f, 1.0f, masterLevel * 3.0f); // 感度を上げてダイナミックに
        
        // 中高音レベル計算（スパイク用） - scopeDataをクランプ
        float midHighLevel = 0.0f;
        int midHighCount = 0;
        for (int i = scopeSize / 4; i < scopeSize / 2; ++i) {
            midHighLevel += std::max(0.0f, scopeData[i]);
            midHighCount++;
        }
        if (midHighCount > 0) midHighLevel /= (float)midHighCount;
        midHighLevel = juce::jlimit(0.0f, 1.0f, midHighLevel * 4.0f);

        // パーティクルを先に描画（ブラックホールに吸い込まれる演出）
        drawParticles(g, centre, maxParticleDist, masterLevel);

        // --- 2. Black Hole Core (Eclipse Style) ---
        // scopeDataは負になる可能性があるのでクランプ
        float bassLevel = juce::jlimit(0.0f, 1.0f, 
            std::max(0.0f, scopeData[0]) * 0.5f + 
            std::max(0.0f, scopeData[1]) * 0.3f + 
            std::max(0.0f, scopeData[2]) * 0.2f);
        
        // ブラックホールのイベントホライズン（黒い核）
        // 低音でサイズが少し変動
        float coreRadius = radius * (0.20f + bassLevel * 0.10f); 
        
        // === 炎/プラズマ風グローエフェクト（円形リング）===
        float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
        
        // 炎グロー（複数層の円形リング）
        for (int layer = 0; layer < 3; ++layer)
        {
            float layerOffset = (float)layer * 0.04f;
            float flameRadius = coreRadius * (1.02f + layerOffset);
            
            // アニメーションするアルファ値（炎のゆらめき）
            float flicker = 0.5f + 0.5f * std::sin(time * 3.0f + layer * 1.5f);
            float baseAlpha = (0.2f - layer * 0.06f) * (0.4f + midHighLevel * 0.6f);
            float layerAlpha = juce::jlimit(0.0f, 0.4f, baseAlpha * (0.7f + flicker * 0.3f));
            
            // 炎の色（内側ほど白、外側ほどオレンジ〜赤）
            juce::Colour flameColor;
            if (layer == 0)
                flameColor = juce::Colour::fromFloatRGBA(1.0f, 0.95f, 0.9f, layerAlpha);   // 白〜クリーム
            else if (layer == 1)
                flameColor = juce::Colour::fromFloatRGBA(1.0f, 0.75f, 0.4f, layerAlpha);  // オレンジ
            else
                flameColor = juce::Colour::fromFloatRGBA(1.0f, 0.5f, 0.2f, layerAlpha);   // 赤オレンジ
            
            g.setColour(flameColor);
            float strokeWidth = 2.5f - layer * 0.6f;
            g.drawEllipse(centre.x - flameRadius, centre.y - flameRadius, 
                         flameRadius * 2.0f, flameRadius * 2.0f, strokeWidth);
        }
        
        // イベントホライズン（本体） - 外周に向かって透けるグラデーション（シアン混ぜ）
        {
            // 背景に馴染むようシアンを少し混ぜた暗い色
            juce::Colour cyanBlack = juce::Colour::fromRGB(5, 15, 20);  // 暗いシアン系
            
            juce::ColourGradient blackHoleGradient(
                juce::Colours::black,  // 中心色（完全不透明）
                centre.x, centre.y,
                cyanBlack.withAlpha(0.0f),  // 外周色（シアン混じりで透明）
                centre.x + coreRadius * 1.2f, centre.y,
                true  // ラジアルグラデーション
            );
            // 中心からのフェードを調整
            blackHoleGradient.addColour(0.5, juce::Colours::black.withAlpha(0.98f));  // 中間点は濃い
            blackHoleGradient.addColour(0.7, cyanBlack.withAlpha(0.85f));  // シアンが少し見え始める
            blackHoleGradient.addColour(0.85, cyanBlack.withAlpha(0.5f));  // 外周に近づくと透け始める
            blackHoleGradient.addColour(0.95, cyanBlack.withAlpha(0.2f));  // 外周でさらに透ける
            
            g.setGradientFill(blackHoleGradient);
            g.fillEllipse(centre.x - coreRadius * 1.2f, centre.y - coreRadius * 1.2f, 
                         coreRadius * 2.4f, coreRadius * 2.4f);
        }
        
        // 追加の闘（中心をより深く見せる）
        g.setColour(juce::Colours::black);
        g.fillEllipse(centre.x - coreRadius*0.7f, centre.y - coreRadius*0.7f, coreRadius * 1.4f, coreRadius * 1.4f);

        // 極細の光輪
        float coronaAlpha = juce::jlimit(0.05f, 0.3f, 0.1f + masterLevel * 0.15f);
        g.setColour(juce::Colours::white.withAlpha(coronaAlpha));
        g.drawEllipse(centre.x - coreRadius*1.01f, centre.y - coreRadius*1.01f, coreRadius * 2.02f, coreRadius * 2.02f, 0.8f);

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
            
            // 🔊 低音連動のジッター（波形全体が揺れる）
            juce::Random& rng = juce::Random::getSystemRandom();
            
            // 🔊 低音連動のジッター（位置揺れ）
            float jitterAmount = bassLevel * 0.5f; // さらに控えめに調整: 0〜0.5ピクセル
            float jitterX = jitterAmount * (rng.nextFloat() - 0.5f);
            float jitterY = jitterAmount * (rng.nextFloat() - 0.5f);
            
            // 🎵 高音連動の微小回転（スピン揺れ）
            float spinAmount = midHighLevel * 0.002f; // さらに控えめに調整
            float spin = spinAmount * (rng.nextFloat() - 0.5f);
            
            auto transform = juce::AffineTransform::rotation(spin)
                                                   .scaled(finalScale, finalScale)
                                                   .translated(centre.x + jitterX, centre.y + jitterY);
            
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
            
            // === プレイヘッド位置ハイライト + RMS振動 ===
            // プレイヘッド付近のセグメントを強調
            if (currentPlayHeadPos >= 0.0f && wp.segmentAngles.size() > 1) // 全レイヤーに適用
            {
                // 角度を 0 ~ 2PI に正規化しつつ、12時基準(-PI/2)に合わせる
                float playHeadAngleRaw = currentPlayHeadPos * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
                float playHeadAngle = std::fmod(playHeadAngleRaw, juce::MathConstants<float>::twoPi);
                if (playHeadAngle < 0) playHeadAngle += juce::MathConstants<float>::twoPi;
                
                float highlightRange = 0.15f; // プレイヘッド前後の強調範囲（ラジアン）
                
                juce::Random& rng = juce::Random::getSystemRandom();
                
                for (size_t seg = 1; seg < wp.segmentAngles.size(); ++seg)
                {
                    float angle1 = wp.segmentAngles[seg - 1];
                    float angle2 = wp.segmentAngles[seg];
                    float rms1 = wp.segmentRms[seg - 1];
                    float rms2 = wp.segmentRms[seg];
                    float inner1 = wp.segmentInnerR[seg - 1];
                    float inner2 = wp.segmentInnerR[seg];
                    float outer1 = wp.segmentOuterR[seg - 1];
                    float outer2 = wp.segmentOuterR[seg];
                    
                    // プレイヘッドからの角度距離を計算 (最短距離ロジック)
                    float midAngle = (angle1 + angle2) * 0.5f;
                    float angleDiff = midAngle - playHeadAngle;
                    
                    // -PI ~ +PI の範囲に正規化して最短距離をとる
                    while (angleDiff < -juce::MathConstants<float>::pi) angleDiff += juce::MathConstants<float>::twoPi;
                    while (angleDiff > juce::MathConstants<float>::pi)  angleDiff -= juce::MathConstants<float>::twoPi;
                    angleDiff = std::abs(angleDiff);
                    
                    // ハイライト強度（距離が近いほど強い）
                    float highlightIntensity = juce::jmax(0.0f, 1.0f - angleDiff / highlightRange);
                    
                    // リアルタイム音量連動の振動
                    // 基本振動（常時）+ 音量連動で振幅増加
                    // 以前ほど派手ではないが、視認できるレベルに戻す
                    float baseVibration = 0.008f; 
                    float audioVibration = masterLevel * 0.15f; 
                    float totalVibration = (baseVibration + audioVibration) * (rng.nextFloat() - 0.5f);
                    
                    // 全セグメントに振動を適用
                    {
                        float r1 = (inner1 + outer1) * 0.5f + totalVibration;
                        float r2 = (inner2 + outer2) * 0.5f + totalVibration;
                        
                        float x1 = centre.x + r1 * finalScale * std::cos(angle1);
                        float y1 = centre.y + r1 * finalScale * std::sin(angle1);
                        float x2 = centre.x + r2 * finalScale * std::cos(angle2);
                        float y2 = centre.y + r2 * finalScale * std::sin(angle2);
                        
                        // 振動ライン（音量に応じて太さと透明度が変化）
                        // 白くなりすぎないようアルファ値を抑えめに調整
                        float vibeAlpha = 0.15f + masterLevel * 0.5f; // 少し戻す
                        float vibeThickness = 1.0f + masterLevel * 3.0f; // 少し戻す
                        g.setColour(wp.colour.brighter(0.5f).withAlpha(juce::jlimit(0.0f, 0.6f, vibeAlpha)));
                        g.drawLine(x1, y1, x2, y2, vibeThickness);
                    }
                    
                    // プレイヘッド付近のハイライト（復活させるが、以前より控えめに）
                    // ユーザーが「白いのはいらない」と言ったのは「太すぎる白線」のことだと推測されるため
                    // 色味を波形カラーベースにし、太さを控えめにして復活させる
                    if (highlightIntensity > 0.0f)
                    {
                        // 以前: 2.0 + 6.0
                        float thickness = 2.0f + highlightIntensity * 4.0f; 
                        
                        // 以前: white.withAlpha(0.8) -> 白すぎて浮いていた
                        // 修正: 波形の色を極端に明るくしたものを使用し、馴染ませる
                        float extraAlpha = highlightIntensity * 0.6f;
                        
                        float r1 = (inner1 + outer1) * 0.5f;
                        float r2 = (inner2 + outer2) * 0.5f;
                        
                        float x1 = centre.x + r1 * finalScale * std::cos(angle1);
                        float y1 = centre.y + r1 * finalScale * std::sin(angle1);
                        float x2 = centre.x + r2 * finalScale * std::cos(angle2);
                        float y2 = centre.y + r2 * finalScale * std::sin(angle2);
                        
                        // 完全な白ではなく、波形カラーの超高輝度版を使う
                        g.setColour(wp.colour.brighter(0.9f).withAlpha(juce::jlimit(0.0f, 0.9f, baseAlpha * 0.5f + extraAlpha)));
                        g.drawLine(x1, y1, x2, y2, thickness);
                    }
                }
            }
        }
        
        // --- Draw Playhead ---
        if (currentPlayHeadPos >= 0.0f)
        {
            // プレイヘッドは累積位置（setPlayHeadPositionで計算済み）を使用
            // ★修正: ユーザー要望によりプレイヘッドだけは3時（0度）基準に戻す
            // 波形は12時スタートだが、プレイヘッドは3時スタートという変則配置
            float manualOffset = 0.0f;
            float angle = (currentPlayHeadPos * juce::MathConstants<float>::twoPi) + manualOffset;
            
            // プレイヘッドライン (レーダーのように中心から外へ)
            // 中心部はブラックホールがあるので、その外側から開始する
            // coreRadiusが 0.20f + bass 程度なので、0.25f〜0.3f あたりから開始すれば綺麗
            auto innerPos = centre.getPointOnCircumference(radius * 0.28f, angle);
            auto outerPos = centre.getPointOnCircumference(radius * 1.1f, angle);
            
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), innerPos.x, innerPos.y,
                                                   juce::Colours::white.withAlpha(0.8f), outerPos.x, outerPos.y, false));
            g.drawLine(innerPos.x, innerPos.y, outerPos.x, outerPos.y, 2.0f);

            auto headPos = centre.getPointOnCircumference(radius, angle);
            g.setColour(juce::Colours::white);
            g.fillEllipse(headPos.x - 3.0f, headPos.y - 3.0f, 6.0f, 6.0f);
        }

        // Draw spinning accent rings
        // 'time' は既に286行目で定義済みなので再利用
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
            float level = std::max(0.0f, scopeData[i * 2]); // 負の値をクランプ
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
        
        // スムーズなズームアニメーション - 反応速度を上げる
        zoomScale += (targetZoomScale - zoomScale) * 0.12f;
        
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
        float loopMultiplier = 1.0f; // x2なら2.0、/2なら0.5
        juce::AudioBuffer<float> originalBuffer; // 元の波形データ（再計算用）
        int originalTrackLength = 0;
        int originalMasterLength = 0;
        int originalRecordStart = 0;
        int originalMasterStart = 0;
        
        // セグメント描画用データ（プレイヘッド太さ変化・振動用）
        std::vector<float> segmentAngles;   // 各ポイントの角度
        std::vector<float> segmentRms;      // 各ポイントのRMS値（0-1）
        std::vector<float> segmentInnerR;   // 各ポイントの内側半径（0-1正規化）
        std::vector<float> segmentOuterR;   // 各ポイントの外側半径（0-1正規化）
    };
    std::vector<WaveformPath> waveformPaths;
    
    // multiplier変更時に波形パスを再生成
    void regenerateWaveformPath(WaveformPath& wp, int effectiveTrackLength, int masterLengthSamples)
    {
        const auto* data = wp.originalBuffer.getReadPointer(0);
        const int originalSamples = wp.originalBuffer.getNumSamples();
        if (originalSamples == 0 || masterLengthSamples == 0) return;
        
        const int points = 1024;
        const float maxAmpWidth = 0.3f;
        
        // ループ比率を計算
        double loopRatio = (double)effectiveTrackLength / (double)masterLengthSamples;
        if (loopRatio > 0.95 && loopRatio < 1.05) loopRatio = 1.0;
        
        // 開始角度の計算
        // 開始角度の計算（正負のオフセットに対応）
        long offsetFromMasterStart = (long)wp.originalRecordStart - (long)wp.originalMasterStart;
        double startAngleRatio = 0.0;
        if (masterLengthSamples > 0)
        {
            // 正負にかかわらず剰余を計算し、0.0~1.0の範囲に正規化
            long relativeStartSample = offsetFromMasterStart % masterLengthSamples;
            startAngleRatio = (double)relativeStartSample / (double)masterLengthSamples;
        }
        
        // ★修正: ユーザー要望により12時基準（-90度）に戻す
        double manualOffset = -juce::MathConstants<double>::halfPi;
        
        juce::Path newPath;
        
        // リピート係数の計算
        // maxMultiplier: 全トラック中の最大倍率（最長トラック）
        // loopMultiplier: このトラックの倍率
        // 最長トラックを基準として、短いトラックは繋げて表示
        // リピート回数 = maxMultiplier / loopMultiplier
        // 例: x2が最長の場合、x2=1回、x1=2回、/2=4回
        
        double repeatFactor = 1.0;
        if (wp.loopMultiplier > 0.0f && maxMultiplier > 0.0f)
        {
            repeatFactor = (double)maxMultiplier / (double)wp.loopMultiplier;
        }
        
        // わずかな誤差は丸める（例: 2.0001 -> 2.0, 0.9999 -> 1.0）
        if (std::abs(repeatFactor - std::round(repeatFactor)) < 0.01)
        {
            repeatFactor = std::round(repeatFactor);
        }
        
        // セグメントデータをクリアして再生成
        wp.segmentAngles.clear();
        wp.segmentRms.clear();
        wp.segmentInnerR.clear();
        wp.segmentOuterR.clear();
        wp.segmentAngles.reserve(points + 1);
        wp.segmentRms.reserve(points + 1);
        wp.segmentInnerR.reserve(points + 1);
        wp.segmentOuterR.reserve(points + 1);
        
        // 1周分の表示で、サンプルをrepeatFactor回繰り返し読む
        // 角度は常に0〜2π（1周）
        for (int i = 0; i <= points; ++i)
        {
            double progressRaw = (double)i / (double)points;
            
            // サンプル位置
            double sampleProgress = std::fmod(progressRaw * repeatFactor, 1.0);
            int startSample = (int)(sampleProgress * wp.originalTrackLength);
            startSample = juce::jmin(startSample, originalSamples - 1);
            
            int samplesToAverage = juce::jmax(1, (int)(wp.originalTrackLength / points));
            float rms = 0.0f;
            for (int j = 0; j < samplesToAverage; ++j)
            {
                int idx = (startSample + j) % originalSamples;
                rms += std::abs(data[idx]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);
            
            // 角度計算：常に1周（0〜2π）
            double currentAngleRatio = startAngleRatio + progressRaw;
            double angleVal = juce::MathConstants<double>::twoPi * currentAngleRatio + manualOffset;
            float angle = (float)angleVal;
            
            float rInner = juce::jmax(0.1f, 1.0f - (rms * maxAmpWidth));
            float rOuter = 1.0f + (rms * maxAmpWidth);
            
            // セグメントデータを保存
            wp.segmentAngles.push_back(angle);
            wp.segmentRms.push_back(rms);
            wp.segmentInnerR.push_back(rInner);
            wp.segmentOuterR.push_back(rOuter);
            
            float xIn = rInner * std::cos(angle);
            float yIn = rInner * std::sin(angle);
            
            if (i == 0) 
                newPath.startNewSubPath(xIn, yIn);
            else
                newPath.lineTo(xIn, yIn);
        }
        
        // 外側のポイントを逆順に追加
        for (int i = points; i >= 0; --i)
        {
            double progressRaw = (double)i / (double)points;
            
            double sampleProgress = std::fmod(progressRaw * repeatFactor, 1.0);
            int startSample = (int)(sampleProgress * wp.originalTrackLength);
            startSample = juce::jmin(startSample, originalSamples - 1);
            
            int samplesToAverage = juce::jmax(1, (int)(wp.originalTrackLength / points));
            float rms = 0.0f;
            for (int j = 0; j < samplesToAverage; ++j)
            {
                int idx = (startSample + j) % originalSamples;
                rms += std::abs(data[idx]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);
            
            double currentAngleRatio = startAngleRatio + progressRaw;
            double angleVal = juce::MathConstants<double>::twoPi * currentAngleRatio + manualOffset;
            float angle = (float)angleVal;
            
            float rOuter = 1.0f + (rms * maxAmpWidth);
            float xOut = rOuter * std::cos(angle);
            float yOut = rOuter * std::sin(angle);
            
            newPath.lineTo(xOut, yOut);
        }
        
        newPath.closeSubPath();
        wp.path = newPath;
    }
    
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
    float lastPlayHeadPos = 0.0f;
    int loopCount = 0;
    float activeMultiplier = 1.0f;  // 現在の倍率（表示用）
    float maxMultiplier = 1.0f;     // 全トラック中の最大倍率（最長トラック基準）
    

    
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
        // scopeDataが負にならないようクランプ
        float bassLevel = juce::jlimit(0.0f, 1.0f, 
            std::max(0.0f, scopeData[0]) * 0.5f + std::max(0.0f, scopeData[1]) * 0.5f);
        float attractStrength = 0.3f + bassLevel * 0.5f; // 低音に反応して吸引力が強くなる
        
        // ベースの力 (通常時ゆっくり) + ドラッグによる追加力
        // dragVelocityRemainingが正なら収束加速、負なら拡散
        // パーティクルの反応速度を上げるため係数を増加
        float forceMultiplier = (dragVelocityRemaining < 0) ? 0.06f : 0.07f;
        float currentAdditionalForce = dragVelocityRemaining * forceMultiplier;
        // 極端な値にならないようクランプ（範囲も拡大）
        currentAdditionalForce = juce::jlimit(-1.2f, 1.5f, currentAdditionalForce);
        
        // ループ外でキャッシュ（パフォーマンス最適化）
        bool isDiffusing = (dragVelocityRemaining < -0.1f);
        float screenMax = (float)juce::jmax(getWidth(), getHeight());
        float outOfBoundsRadius = isDiffusing ? screenMax * 3.0f : screenMax * 1.5f;
        
        for (int i = 0; i < numParticles; ++i)
        {
            float dist = std::sqrt(particles[i].x * particles[i].x + particles[i].y * particles[i].y);
            
            float totalForce = (attractStrength * 0.015f) + currentAdditionalForce;
            
            if (dist > 1.0f)
            {
                // 中心に向かうベクトルを計算
                float dirX = -particles[i].x / dist;
                float dirY = -particles[i].y / dist;
                
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
            
            // ========================================
            // シンプルなリセットロジック
            // ========================================
            
            // 中心到達でリセット
            if (dist < 10.0f)
            {
                resetParticle(i);
                continue;
            }
            
            // 画面外到達でリセット
            if (dist > screenMax * 1.2f)
            {
                resetParticle(i);
                continue;
            }
            
            // ドラッグ中はパーティクルをより頻繁にリスポーン（生成速度加速）
            if (std::abs(dragVelocityRemaining) > 0.5f)
            {
                // ドラッグ強度に応じてリスポーン確率を上げる
                float respawnChance = std::abs(dragVelocityRemaining) * 0.02f;
                if (juce::Random::getSystemRandom().nextFloat() < respawnChance)
                {
                    resetParticle(i);
                    continue;
                }
            }
            
            // ========================================
            // 距離ベースの透明度
            // 中心に近いほど透明、画面端に近いほど透明
            // 中間地点で最も不透明
            // ========================================
            float centerFade = juce::jlimit(0.0f, 1.0f, dist / 100.0f); // 中心から100pxで完全不透明
            float edgeFade = juce::jlimit(0.0f, 1.0f, 1.0f - (dist / (screenMax * 1.2f))); // 端から200pxでフェード開始
            particles[i].alpha = juce::jlimit(0.0f, 1.0f, centerFade * edgeFade * 0.8f);
            
            // サイズの安全クランプ
            particles[i].size = juce::jmax(0.5f, particles[i].size);
        }
        
        // ドラッグ力の減衰 (慣性) - 素早く減衰して反応を鋭くする
        dragVelocityRemaining *= 0.85f;
        if (std::abs(dragVelocityRemaining) < 0.001f) dragVelocityRemaining = 0.0f;
    }

    void drawParticles(juce::Graphics& g, juce::Point<float> centre, float maxRadius, float audioLevel)
    {
        // 0 除算防止
        if (maxRadius < 1.0f) maxRadius = 400.0f;
        
        for (int i = 0; i < numParticles; ++i)
        {
            float px = centre.x + particles[i].x;
            float py = centre.y + particles[i].y;
            float dist = std::sqrt(particles[i].x * particles[i].x + particles[i].y * particles[i].y);
            
            if (dist > maxRadius * 1.5f) continue;
            
            // 中心に近いほど明るく
            float proximityBonus = juce::jlimit(0.0f, 1.0f, 1.0f - (dist / maxRadius));
            float alpha = juce::jlimit(0.0f, 1.0f, particles[i].alpha * particles[i].life * (0.2f + proximityBonus * 0.6f));
            
            // 音量レベルによるブースト（アルファ値）
            // 音が大きいと不透明度が上がる
            float alphaBoost = audioLevel * 0.5f;
            alpha = juce::jlimit(0.0f, 1.0f, alpha * (1.0f + alphaBoost));

            // パーティクル本体（核） - さらに小さく
            g.setColour(juce::Colours::white.withAlpha(alpha));
            float coreSize = particles[i].size * 0.4f; 
            
            // サイズも音量で少し大きく
            float sizeBoost = 1.0f + audioLevel * 0.5f;
            coreSize *= sizeBoost;
            
            g.fillEllipse(px - coreSize*0.5f, py - coreSize*0.5f, coreSize, coreSize);
            
            // スモーク（柔らかいグロー）
            // サイズを小さくして繊細に（細く）
            juce::Colour smokeColor = juce::Colour::fromFloatRGBA(0.85f, 0.9f, 1.0f, 1.0f);
            
            // 音量が大きいと少し白さを強調
            if (audioLevel > 0.5f) {
                smokeColor = smokeColor.brighter(0.1f * (audioLevel - 0.5f));
            }
            
            float glowAlpha = juce::jlimit(0.0f, 1.0f, alpha * 0.25f); 
            g.setColour(smokeColor.withAlpha(glowAlpha));
            
            // 倍率を下げる: 2.5 -> 1.4 -> 音量でブースト
            float smokeSize = particles[i].size * 1.4f * sizeBoost;
            g.fillEllipse(px - smokeSize*0.5f, py - smokeSize*0.5f, smokeSize, smokeSize);
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
