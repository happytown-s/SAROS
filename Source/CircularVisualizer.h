#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "ThemeColours.h"

class CircularVisualizer : public juce::Component, 
                           public juce::Timer
{
public:
    CircularVisualizer()
        : forwardFFT(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        // 背景はMainComponentのGLレンダリングに任せるため透明にする
        setOpaque(false);
        startTimerHz(60);
        setInterceptsMouseClicks(true, true);
        
        for (int i = 0; i < numParticles; ++i)
            resetParticle(i);
    }
    
    ~CircularVisualizer() override
    {
    }
    
    // OpenGL Resources Initialization (called from MainComponent)
    void initGL(juce::OpenGLContext& context)
    {
        DBG("✨ Visualizer GL Resources Initializing");
        using namespace juce::gl;
        
        glowShader = std::make_unique<juce::OpenGLShaderProgram>(context);
        
        juce::String vertexCode =
            "attribute vec2 position;\n"
            "varying vec2 vUv;\n"
            "void main() {\n"
            "    vUv = position * 0.5 + 0.5;\n"
            "    gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";
        
        juce::String fragmentCode =
            "varying vec2 vUv;\n"
            "uniform float time;\n"
            "uniform float masterRMS;\n"
            "uniform vec3 glowColor;\n"
            "void main() {\n"
            "    vec2 center = vec2(0.5, 0.5);\n"
            "    float dist = distance(vUv, center);\n"
            "    float glowRadius = 0.3 + masterRMS * 0.2;\n"
            "    float glow = exp(-dist * dist / (glowRadius * glowRadius * 0.1));\n"
            "    float pulse = 1.0 + sin(time * 3.0) * 0.1 * masterRMS;\n"
            "    glow *= pulse;\n"
            "    vec3 color = glowColor * glow;\n"
            "    gl_FragColor = vec4(color, glow * 0.5);\n"
            "}\n";
        
        if (!glowShader->addVertexShader(vertexCode) ||
            !glowShader->addFragmentShader(fragmentCode) ||
            !glowShader->link())
        {
            DBG("Shader error: " + glowShader->getLastError());
            glowShader.reset();
            return;
        }
        DBG("✅ Glow shader compiled");
        
        float quadVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        GLint posAttr = glGetAttribLocation(glowShader->getProgramID(), "position");
        if (posAttr >= 0) {
            glEnableVertexAttribArray((GLuint)posAttr);
            glVertexAttribPointer((GLuint)posAttr, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        }
        glBindVertexArray(0);
        
        // 波形シェーダー
        waveformShader = std::make_unique<juce::OpenGLShaderProgram>(context);
        
        juce::String waveVertexCode =
            "attribute vec2 position;\n"
            "attribute vec4 color;\n"
            "varying vec4 vColor;\n"
            "void main() {\n"
            "    vColor = color;\n"
            "    gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";
        
        juce::String waveFragmentCode =
            "varying vec4 vColor;\n"
            "void main() {\n"
            "    gl_FragColor = vColor;\n"
            "}\n";
        
        if (!waveformShader->addVertexShader(waveVertexCode) ||
            !waveformShader->addFragmentShader(waveFragmentCode) ||
            !waveformShader->link())
        {
            DBG("Waveform shader error: " + waveformShader->getLastError());
            waveformShader.reset();
            return;
        }
        DBG("✅ Waveform shader compiled");
        
        // 波形VBO/VAO (動的データ用)
        glGenVertexArrays(1, &waveformVao);
        glGenBuffers(1, &waveformVbo);
        
        // ブラックホールシェーダー
        blackHoleShader = std::make_unique<juce::OpenGLShaderProgram>(context);
        
        juce::String bhVertexCode =
            "attribute vec2 position;\n"
            "varying vec2 vUv;\n"
            "void main() {\n"
            "    vUv = position * 0.5 + 0.5;\n"
            "    gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";
        
        juce::String bhFragmentCode =
            "varying vec2 vUv;\n"
            "uniform float time;\n"
            "uniform float bassLevel;\n"
            "uniform float midHighLevel;\n"
            "uniform float aspectRatio;\n"
            "void main() {\n"
            "    vec2 center = vec2(0.5, 0.5);\n"
            "    vec2 uv = vUv;\n"
            "    uv.x = (uv.x - 0.5) * aspectRatio + 0.5;\n"
            "    float dist = distance(uv, center);\n"
            "    float coreRadius = 0.07 + bassLevel * 0.02;\n"
            "    float innerRadius = coreRadius * 0.7;\n"
            "    \n"
            "    // Black hole core\n"
            "    if (dist < innerRadius) {\n"
            "        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "        return;\n"
            "    }\n"
            "    \n"
            "    // Event horizon (fade out)\n"
            "    if (dist < coreRadius) {\n"
            "        float t = (dist - innerRadius) / (coreRadius - innerRadius);\n"
            "        float alpha = 1.0 - t * t;\n"
            "        gl_FragColor = vec4(0.0, 0.0, 0.0, alpha);\n"
            "        return;\n"
            "    }\n"
            "    \n"
            "    // Flame ring\n"
            "    float flameRadius = coreRadius * 1.05;\n"
            "    float flameDist = abs(dist - flameRadius);\n"
            "    float flicker = 0.5 + 0.5 * sin(time * 3.0);\n"
            "    float flame = exp(-flameDist * flameDist * 800.0) * (0.4 + midHighLevel * 0.6) * (0.7 + flicker * 0.3);\n"
            "    vec3 flameColor = mix(vec3(1.0, 0.5, 0.2), vec3(1.0, 0.95, 0.9), flame);\n"
            "    \n"
            "    // Glow halo\n"
            "    float glowDist = abs(dist - innerRadius);\n"
            "    float glow = exp(-glowDist * glowDist * 200.0) * 0.5;\n"
            "    \n"
            "    vec3 finalColor = flameColor * flame + vec3(1.0) * glow;\n"
            "    float finalAlpha = max(flame, glow);\n"
            "    gl_FragColor = vec4(finalColor, finalAlpha);\n"
            "}\n";
        
        if (!blackHoleShader->addVertexShader(bhVertexCode) ||
            !blackHoleShader->addFragmentShader(bhFragmentCode) ||
            !blackHoleShader->link())
        {
            DBG("BlackHole shader error: " + blackHoleShader->getLastError());
            blackHoleShader.reset();
        }
        else
        {
            DBG("✅ BlackHole shader compiled");
        }
    }
    
    void cleanupGL()
    {
        DBG("🟠 Visualizer GL Resources Closing");
        using namespace juce::gl;
        if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao != 0) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (waveformVbo != 0) { glDeleteBuffers(1, &waveformVbo); waveformVbo = 0; }
        if (waveformVao != 0) { glDeleteVertexArrays(1, &waveformVao); waveformVao = 0; }
        glowShader.reset();
        waveformShader.reset();
        blackHoleShader.reset();
    }
    
    void renderGLCombined(juce::OpenGLContext& context)
    {
        using namespace juce::gl;
        
        // 注意: 背景のクリア(glClear)はMainComponent側で行うため、ここでは行わない
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // グロー効果描画
        if (glowShader) {
            glowShader->use();
            float t = (float)juce::Time::getMillisecondCounterHiRes() / 1000.0f;
            glowShader->setUniform("time", t);
            glowShader->setUniform("masterRMS", currentMasterRMS);
            glowShader->setUniform("glowColor", 0.0f, 0.8f, 0.8f);
            
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }
        
        // ブラックホール描画
        if (blackHoleShader) {
            blackHoleShader->use();
            float t = (float)juce::Time::getMillisecondCounterHiRes() / 1000.0f;
            float aspectRatio = (float)getWidth() / (float)juce::jmax(1, getHeight());
            blackHoleShader->setUniform("time", t);
            blackHoleShader->setUniform("bassLevel", currentBassLevel);
            blackHoleShader->setUniform("midHighLevel", currentMidHighLevel);
            blackHoleShader->setUniform("aspectRatio", aspectRatio);
            
            glBindVertexArray(vao);  // 同じ全画面クワッドを再利用
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }
        
        // 波形描画
        if (waveformShader && !glWaveformData.empty()) {
            waveformShader->use();
            glLineWidth(2.0f);  // 線の太さ
            
            for (const auto& vertices : glWaveformData) {
                if (vertices.empty()) continue;
                
                glBindBuffer(GL_ARRAY_BUFFER, waveformVbo);
                glBufferData(GL_ARRAY_BUFFER, 
                             vertices.size() * sizeof(WaveformGLVertex),
                             vertices.data(), GL_DYNAMIC_DRAW);
                
                glBindVertexArray(waveformVao);
                
                GLint posAttr = glGetAttribLocation(waveformShader->getProgramID(), "position");
                GLint colAttr = glGetAttribLocation(waveformShader->getProgramID(), "color");
                
                if (posAttr >= 0) {
                    glEnableVertexAttribArray((GLuint)posAttr);
                    glVertexAttribPointer((GLuint)posAttr, 2, GL_FLOAT, GL_FALSE, 
                                          sizeof(WaveformGLVertex), (void*)0);
                }
                if (colAttr >= 0) {
                    glEnableVertexAttribArray((GLuint)colAttr);
                    glVertexAttribPointer((GLuint)colAttr, 4, GL_FLOAT, GL_FALSE,
                                          sizeof(WaveformGLVertex), 
                                          (void*)(2 * sizeof(float)));
                }
                
                glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)vertices.size());
                glBindVertexArray(0);
            }
        }
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

        // 全トラック12時スタート（readPosition 0 が12時に来る）
        double startAngleRatio = 0.0;

        juce::Path newPath;
        const float maxAmpWidth = 0.3f;
        double sampleStep = (double)numSamples / (double)points;
        
        // ★ オフセット設定: 12時基準（-halfPi）
        double manualOffset = -juce::MathConstants<double>::halfPi;

        for (int i = 0; i <= points; ++i)
        {
            float rms = 0.0f;
            double startSampleRaw = i * sampleStep;
            int startSample = (int)startSampleRaw;
            int samplesToAverage = (int)sampleStep;
            if (samplesToAverage < 1) samplesToAverage = 1;
            for (int j = 0; j < samplesToAverage; ++j) {
                if (startSample + j < numSamples) rms += std::abs(data[startSample + j]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);

            double progressRaw = (double)i / (double)points;
            double currentAngleRatio = startAngleRatio + (progressRaw * loopRatio / maxMultiplier);
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
            float rms = 0.0f;
            double startSampleRaw = i * sampleStep;
            int startSample = (int)startSampleRaw;
            int samplesToAverage = (int)sampleStep;
            if (samplesToAverage < 1) samplesToAverage = 1;
            for (int j = 0; j < samplesToAverage; ++j) {
                if (startSample + j < numSamples) rms += std::abs(data[startSample + j]);
            }
            rms /= (float)samplesToAverage;
            rms = std::pow(rms, 0.6f);

            double progressRaw = (double)i / (double)points;
            double currentAngleRatio = startAngleRatio + (progressRaw * loopRatio / maxMultiplier);
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
        
        updateGLWaveformData();  // GL頂点データを更新
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
        updateGLWaveformData();  // GL頂点データを更新
        repaint();
    }
    
    // GL頂点データを更新（WaveformPathから変換）
    void updateGLWaveformData()
    {
        glWaveformData.clear();
        
        float width = (float)getWidth();
        float height = (float)getHeight();
        if (width < 1.0f || height < 1.0f) return;
        
        float minDim = juce::jmin(width, height);
        float centerX = width * 0.5f;
        float centerY = height * 0.5f;
        float baseRadius = minDim * 0.35f;  // 基準半径
        
        for (const auto& wp : waveformPaths)
        {
            std::vector<WaveformGLVertex> vertices;
            
            // 色を正規化
            float r = wp.colour.getFloatRed();
            float g = wp.colour.getFloatGreen();
            float b = wp.colour.getFloatBlue();
            float a = wp.colour.getFloatAlpha() * wp.spawnProgress;
            
            // originalBufferから波形を再計算してGL頂点を生成
            if (wp.originalBuffer.getNumSamples() > 0)
            {
                const int points = 512;
                const auto* data = wp.originalBuffer.getReadPointer(0);
                const int numSamples = wp.originalBuffer.getNumSamples();
                double sampleStep = (double)numSamples / (double)points;
                
                double loopRatio = wp.loopMultiplier;
                double manualOffset = -juce::MathConstants<double>::halfPi;
                
                for (int i = 0; i <= points; ++i)
                {
                    float rms = 0.0f;
                    int startSample = (int)(i * sampleStep);
                    int samplesToAvg = juce::jmax(1, (int)sampleStep);
                    for (int j = 0; j < samplesToAvg && startSample + j < numSamples; ++j)
                        rms += std::abs(data[startSample + j]);
                    rms /= (float)samplesToAvg;
                    rms = std::pow(rms, 0.6f);
                    
                    double progressRaw = (double)i / (double)points;
                    double angle = juce::MathConstants<double>::twoPi * (progressRaw * loopRatio / maxMultiplier) + manualOffset;
                    
                    // 内側の線として描画
                    float radius = baseRadius * (1.0f - rms * 0.3f);
                    float px = (centerX + radius * (float)std::cos(angle)) / width * 2.0f - 1.0f;
                    float py = (centerY + radius * (float)std::sin(angle)) / height * 2.0f - 1.0f;
                    py = -py;  // Y軸反転
                    
                    vertices.push_back({px, py, r, g, b, a});
                }
            }
            
            if (!vertices.empty())
                glWaveformData.push_back(std::move(vertices));
        }
    }

    void setTrackMultiplier(int trackId, float multiplier)
    {
        DBG("🔍 setTrackMultiplier: trackId=" << trackId << " multiplier=" << multiplier);
        for (auto& wp : waveformPaths)
        {
            if (wp.trackId == trackId)
                wp.loopMultiplier = multiplier;
        }
    }

    // トラックごとのRMSを更新（物理演算のターゲット）
    void updateTrackRMS(int trackId, float rms)
    {
        for (auto& wp : waveformPaths)
        {
            if (wp.trackId == trackId)
            {
                wp.targetRms = rms;
                break;
            }
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
    // 累積位置を直接受け取る（LooperAudio::getEffectiveNormalizedPositionから）
    void setPlayHeadPosition(float effectiveNormalizedPos)
    {
        currentPlayHeadPos = effectiveNormalizedPos;
    }
    
    void resetPlayHead()
    {
        loopCount = 0;
        lastPlayHeadPos = 0.0f;
        currentPlayHeadPos = -1.0f;
    }

    // ==========================================
    // Video Mode Animation (Breathing)
    // ==========================================
    void setVideoMode(bool isEnabled)
    {
        isVideoMode = isEnabled;
        if (!isEnabled)
        {
            // Reset to default
            videoZoomFactor = 1.0f;
        }
    }

    // progress: 0.0 (Start) -> 0.5 (Max Scale) -> 1.0 (End)
    void setVideoAnimationProgress(float progress)
    {
        if (!isVideoMode) return;

        // Breath Animation: Sine wave-like curve
        // 0.0 -> 1.0 -> 0.0
        // sin(0) -> sin(PI) -> sin(2PI) is not typical breath
        // Linear Triangle: 0->1->0
        
        float normalized;
        if (progress < 0.5f)
        {
            // 0.0 -> 0.5 => 0.0 -> 1.0
            // Ease In/Out
             normalized = (1.0f - std::cos(progress * 2.0f * juce::MathConstants<float>::pi)) * 0.5f; // 0 to 1??? No.
             // Simple sine: sin(progress * PI) -> 0 to 1 to 0
             normalized = std::sin(progress * juce::MathConstants<float>::pi);
        }
        else
        {
             normalized = std::sin(progress * juce::MathConstants<float>::pi);
        }
        
        // Map 0.0~1.0 to 1.0x ~ 2.0x (Max Scale)
        const float maxVideoZoom = 1.8f; 
        videoZoomFactor = 1.0f + normalized * (maxVideoZoom - 1.0f);
        
        // Override targetZoomScale to apply immediately in timer? 
        // Or just use videoZoomFactor in paint?
        // Let's use videoZoomFactor as a multiplier on top of base scale.
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // ★ OpenGL が背景をクリアするのでfillAll は不要
        // ★ 正方形領域を強制して楕円歪みを防止
        float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto squareArea = bounds.withSizeKeepingCentre(side, side);
        auto centre = squareArea.getCentre();
        auto radius = side * 0.35f;
        if (radius <= 0) return;

        // --- Visualizer Elements (Overlay only) ---
        // 背景円はGLで描画されるため削除
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
        
        // 低音レベル計算（ブラックホール用）
        float bassLevel = juce::jlimit(0.0f, 1.0f, 
            std::max(0.0f, scopeData[0]) * 0.5f + 
            std::max(0.0f, scopeData[1]) * 0.3f + 
            std::max(0.0f, scopeData[2]) * 0.2f);
        
        // GLシェーダー用にメンバー変数に保存
        currentBassLevel = bassLevel;
        currentMidHighLevel = midHighLevel;

        // パーティクルを先に描画（ブラックホールに吸い込まれる演出）
        drawParticles(g, centre, maxParticleDist, masterLevel);

        // --- 2. Black Hole Core ---
        // ★ GLシェーダー (blackHoleShader) で描画されるため削除
        // coreRadius は波形描画で使用するため計算は残す
        float coreRadius = radius * (0.20f + bassLevel * 0.10f);
        float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
        
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
            // Video Mode: Multiply by videoZoomFactor
            float currentZoom = zoomScale;
            if (isVideoMode) currentZoom *= videoZoomFactor;
            
            float zoomedScale = scaleLayer * currentZoom;
            
            // 画面外に大きくなりすぎたら描画スキップ（適当な上限）
            if (zoomedScale > 5.0f) continue;

            // 出現アニメーション適用: newest spawn starts from center (0.0) -> expands to 1.0
            float finalScale = radius * zoomedScale * wp.spawnProgress;
            
            // アルファ値: 古いほど（外側ほど）薄くするフェードアウト
            // i=0 -> 0.9, i=1 -> 0.8...
            float baseAlpha = (0.9f - layerOffset * 0.5f) * wp.spawnProgress;
            if (baseAlpha < 0.0f) baseAlpha = 0.0f;
            
            // 🔊 低音連動のジッター（位置揺れ）
            juce::Random& rng = juce::Random::getSystemRandom();
            
            // 全体的なゆらぎ（位置）
            float globalJitterAmount = bassLevel * 0.5f; 
            float globalJitterX = globalJitterAmount * (rng.nextFloat() - 0.5f);
            float globalJitterY = globalJitterAmount * (rng.nextFloat() - 0.5f);
            
            // スピン
            float spinAmount = midHighLevel * 0.002f;
            float spin = spinAmount * (rng.nextFloat() - 0.5f);

            // ポンプ（RMS連動サイズ変化）
            float pumpScale = 1.0f + (wp.currentRms * 0.02f); 
            float currentTotalScale = finalScale * pumpScale;

            // Transform for global movements
            auto transform = juce::AffineTransform::rotation(spin)
                                                   .scaled(currentTotalScale, currentTotalScale)
                                                   .translated(centre.x + globalJitterX, centre.y + globalJitterY);
            
            // ★ "Ribbon Jitter" (Edges shivering)
            juce::Path ribbonPath;
            
            if (!wp.segmentAngles.empty())
            {
                // ビリビリ感の調整: ユーザー要望により抑えめに
                // bassLevelが高いときだけ震えるが、係数を下げる
                float vibrationIntensity = 0.0f;
                if (bassLevel > 0.15f) {
                     // 以前: (bassLevel - 0.1) * 0.08 -> 修正: 閾値を上げ、係数を半分以下に
                     vibrationIntensity = (bassLevel - 0.15f) * 0.035f; 
                }
                
                // トラックごとの音量連動を強化（個別反応のため）
                // 以前: 0.008f → 修正: 0.02f (2.5倍)
                vibrationIntensity += wp.currentRms * 0.02f; 
                
                // トラック固有のPump（サイズ変化）を強化
                // 以前: 0.15f → 修正: 0.35f (2倍以上)
                float pumpAmount = wp.currentRms * 0.35f;
                // bassLevelは背景程度に抑える (0.08 → 0.03)
                pumpAmount += bassLevel * 0.03f;
                
                // 適用
                currentTotalScale = finalScale * (1.0f + pumpAmount);
                
                // Transformを再生成（スケール変更のため）
                transform = juce::AffineTransform::rotation(spin)
                                       .scaled(currentTotalScale, currentTotalScale)
                                       .translated(centre.x + globalJitterX, centre.y + globalJitterY);

                const size_t numPoints = wp.segmentAngles.size();
                
                // 1. Inner Edge
                for (size_t i = 0; i < numPoints; ++i)
                {
                    float angle = wp.segmentAngles[i];
                    float rInner = wp.segmentInnerR[i];
                    
                    // Jitter applied to inner edge
                    float rJitter = (rng.nextFloat() - 0.5f) * vibrationIntensity;
                    float x = (rInner + rJitter) * std::cos(angle);
                    float y = (rInner + rJitter) * std::sin(angle);
                    
                    if (i == 0) ribbonPath.startNewSubPath(x, y);
                    else        ribbonPath.lineTo(x, y);
                }
                
                // 2. Outer Edge (Reverse order to close shape)
                for (int i = (int)numPoints - 1; i >= 0; --i)
                {
                    float angle = wp.segmentAngles[i];
                    float rOuter = wp.segmentOuterR[i];
                    
                    // Jitter applied to outer edge
                    float rJitter = (rng.nextFloat() - 0.5f) * vibrationIntensity;
                    float x = (rOuter + rJitter) * std::cos(angle);
                    float y = (rOuter + rJitter) * std::sin(angle);
                    
                    ribbonPath.lineTo(x, y);
                }
                
                ribbonPath.closeSubPath();
                ribbonPath.applyTransform(transform);
            }
            else
            {
                // データがない場合は元のパスを使用（フォールバック）
                ribbonPath = wp.path;
                ribbonPath.applyTransform(transform);
            }
            
            // --- Drawing (Ribbon Style) ---
            
            // 1. Fill (Body)
            g.setColour(wp.colour.withAlpha(juce::jlimit(0.2f, 0.6f, baseAlpha)));
            g.fillPath(ribbonPath);
            
            // 2. Edge Glow (Stroke)
            float strokeWidth = 1.0f + masterLevel * 1.5f;
            
            // Inner/Outer glow
            for (int glow = 3; glow >= 1; --glow)
            {
                float glowAlpha = baseAlpha * 0.3f / (float)glow;
                g.setColour(wp.colour.withAlpha(juce::jlimit(0.05f, 0.4f, glowAlpha)));
                g.strokePath(ribbonPath, juce::PathStrokeType(glow * 3.0f));
            }
            
            // Sharp Edge
            g.setColour(wp.colour.brighter(0.8f).withAlpha(juce::jlimit(0.5f, 1.0f, baseAlpha + 0.2f)));
            g.strokePath(ribbonPath, juce::PathStrokeType(1.0f)); 

            
            // === プレイヘッド位置: 波形セグメント自体を光らせる ===
            if (currentPlayHeadPos >= 0.0f && !wp.segmentAngles.empty())
            {
                // 90度（π/2）補正: 時計回りに早かったので引く
                float playHeadAngle = currentPlayHeadPos * juce::MathConstants<float>::twoPi
                                    - juce::MathConstants<float>::halfPi;
                const size_t numPoints = wp.segmentAngles.size();

                // プレイヘッド角度に最も近いセグメントインデックスを見つける
                size_t closestIdx = 0;
                float minDiff = juce::MathConstants<float>::twoPi;
                for (size_t i = 0; i < numPoints; ++i)
                {
                    float diff = std::abs(wp.segmentAngles[i] - playHeadAngle);
                    // 円周の折り返しを考慮
                    if (diff > juce::MathConstants<float>::pi)
                        diff = juce::MathConstants<float>::twoPi - diff;
                    if (diff < minDiff)
                    {
                        minDiff = diff;
                        closestIdx = i;
                    }
                }

                // ハイライト範囲 (前後数セグメント)
                const int highlightWidth = 8;  // 前後8セグメント = 計17セグメント

                // ハイライトセグメントのパスを構築
                juce::Path highlightPath;

                // Inner edge
                for (int offset = -highlightWidth; offset <= highlightWidth; ++offset)
                {
                    int idx = ((int)closestIdx + offset + (int)numPoints) % (int)numPoints;
                    float angle = wp.segmentAngles[idx];
                    float rInner = wp.segmentInnerR[idx];
                    float x = rInner * std::cos(angle);
                    float y = rInner * std::sin(angle);

                    if (offset == -highlightWidth)
                        highlightPath.startNewSubPath(x, y);
                    else
                        highlightPath.lineTo(x, y);
                }

                // Outer edge (reverse)
                for (int offset = highlightWidth; offset >= -highlightWidth; --offset)
                {
                    int idx = ((int)closestIdx + offset + (int)numPoints) % (int)numPoints;
                    float angle = wp.segmentAngles[idx];
                    float rOuter = wp.segmentOuterR[idx];
                    float x = rOuter * std::cos(angle);
                    float y = rOuter * std::sin(angle);
                    highlightPath.lineTo(x, y);
                }

                highlightPath.closeSubPath();
                highlightPath.applyTransform(transform);

                // グロー描画 (複数レイヤー)
                for (int glow = 5; glow >= 1; --glow)
                {
                    float glowAlpha = 0.12f / (float)glow;
                    g.setColour(wp.colour.brighter(0.8f).withAlpha(glowAlpha));
                    g.strokePath(highlightPath, juce::PathStrokeType((float)glow * 4.0f));
                }

                // メインの明るい塗りつぶし
                g.setColour(wp.colour.brighter(1.5f).withAlpha(0.6f));
                g.fillPath(highlightPath);

                // 白い縁取り
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.strokePath(highlightPath, juce::PathStrokeType(1.5f));
            }
        }
        
        // --- Draw Playhead ---
        if (currentPlayHeadPos >= 0.0f)
        {
            float manualOffset = 0.0f;
            float angle = (currentPlayHeadPos * juce::MathConstants<float>::twoPi) + manualOffset;
            
            // プレイヘッドライン (レーダーのように中心から外へ)
            // ユーザー要望により先端の白丸は削除し、さらに長く伸ばす
            
            auto innerPos = centre.getPointOnCircumference(radius * 0.28f, angle);
            auto outerPos = centre.getPointOnCircumference(radius * 1.35f, angle); // 1.1f -> 1.35f
            
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), innerPos.x, innerPos.y,
                                                   juce::Colours::white.withAlpha(0.8f), outerPos.x, outerPos.y, false));
            g.drawLine(innerPos.x, innerPos.y, outerPos.x, outerPos.y, 2.0f);

            // Removed white circle at tip as requested
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
        // Physics constants (Rubber effect)
        const float stiffness = 0.25f;
        const float damping = 0.85f;

        for (auto& wp : waveformPaths)
        {
            // 波形の出現アニメーション (0.15 -> 0.05 ゆっくり)
            if (wp.spawnProgress < 1.0f) {
                wp.spawnProgress += (1.0f - wp.spawnProgress) * 0.05f;
                if (std::abs(1.0f - wp.spawnProgress) < 0.001f) wp.spawnProgress = 1.0f;
            }
            
            // RMS Spring Physics
            float force = (wp.targetRms - wp.currentRms) * stiffness;
            wp.vibrationVelocity += force;
            wp.vibrationVelocity *= damping;
            wp.currentRms += wp.vibrationVelocity;
        }
        
        repaint(); // Always repaint for animations
        
        if (nextFFTBlockReady)
        {
            drawNextFrameOfSpectrum();
            nextFFTBlockReady = false;
        }
    }

private:
    // OpenGL リソース - グロー効果用
    // juce::OpenGLContext openGLContext; // MainComponent 側に集約
    std::unique_ptr<juce::OpenGLShaderProgram> glowShader;
    unsigned int vbo = 0;
    unsigned int vao = 0;
    float currentMasterRMS = 0.0f;
    
    // OpenGL リソース - ブラックホール用
    std::unique_ptr<juce::OpenGLShaderProgram> blackHoleShader;
    float currentBassLevel = 0.0f;
    float currentMidHighLevel = 0.0f;
    
    // OpenGL リソース - 波形描画用
    std::unique_ptr<juce::OpenGLShaderProgram> waveformShader;
    unsigned int waveformVbo = 0;
    unsigned int waveformVao = 0;
    
    // 波形頂点データ (x, y, r, g, b, a)
    struct WaveformGLVertex {
        float x, y;
        float r, g, b, a;
    };
    std::vector<std::vector<WaveformGLVertex>> glWaveformData;  // トラックごとの頂点配列
   
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
        
        // Physics State for Vibration
        float targetRms = 0.0f;
        float currentRms = 0.0f;
        float vibrationVelocity = 0.0f;
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
        
        // 描画比率の計算
        // effectiveTrackLength が0の場合は元のトラック長を使用する（スレーブ表示崩れ対策）
        int usedTrackLength = (effectiveTrackLength > 0) ? effectiveTrackLength : wp.originalTrackLength;
        double loopRatio = (double)usedTrackLength / (double)masterLengthSamples;
        if (loopRatio > 0.95 && loopRatio < 1.05) loopRatio = 1.0;
        
        // 全トラック12時スタート（readPosition 0 が12時に来る）
        double startAngleRatio = 0.0;

        // ★ オフセット設定: 12時基準（-halfPi）
        double manualOffset = -juce::MathConstants<double>::halfPi;
        
        juce::Path newPath;
        
        // リピート回数の計算
        // maxMultiplier: 全トラック中の最大倍率（最長トラック）
        // loopMultiplier: このトラックの倍率
        // 最長トラックを基準として、短いトラックは繋げて表示
        // リピート回数 = maxMultiplier / loopMultiplier
        // 例: x2が最長の場合、x2=1回、x1=2回、/2=4回
        
        double repeatFactor = 1.0;
        if (maxMultiplier > 0 && wp.loopMultiplier > 0)
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
            
            // 角度計算：円周全体の位相 alignment に基づく
            // progressRaw 自体が 0..1 で円周全体の進行度を表す
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
        
        // 外側のラインを追加
        for (int i = points; i >= 0; --i)
        {
            float angle = wp.segmentAngles[i];
            float rOuter = wp.segmentOuterR[i];
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

    // Video Mode State
    bool isVideoMode = false;
    float videoZoomFactor = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularVisualizer)
};
