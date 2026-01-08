#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>

// =====================================================
// キーボードマッピング管理クラス
// =====================================================
class KeyboardMappingManager
{
public:
    // アクションID一覧
    static constexpr const char* ACTION_REC = "rec";
    static constexpr const char* ACTION_PLAY = "play";
    static constexpr const char* ACTION_UNDO = "undo";
    static constexpr const char* ACTION_TRACK_1 = "track_1";
    static constexpr const char* ACTION_TRACK_2 = "track_2";
    static constexpr const char* ACTION_TRACK_3 = "track_3";
    static constexpr const char* ACTION_TRACK_4 = "track_4";
    static constexpr const char* ACTION_TRACK_5 = "track_5";
    static constexpr const char* ACTION_TRACK_6 = "track_6";
    static constexpr const char* ACTION_TRACK_7 = "track_7";
    static constexpr const char* ACTION_TRACK_8 = "track_8";
    static constexpr const char* ACTION_AUTO_ARM = "auto_arm";
    static constexpr const char* ACTION_VISUAL_MODE = "visual_mode";
    static constexpr const char* ACTION_FX_MODE = "fx_mode";
    
    // FXトグルアクションはgetAllActions()で動的生成
    // パターン: fx_t{trackId}_slot{slotId}_bypass, fx_t{trackId}_filter_type, fx_t{trackId}_repeat_active
    
    KeyboardMappingManager()
    {
        // 設定ファイル初期化
        juce::PropertiesFile::Options options;
        options.applicationName = "SAROS";
        options.filenameSuffix = ".keymap";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = "SAROS";
        
        propertiesFile.reset(new juce::PropertiesFile(options));
        loadMappings();
    }
    
    ~KeyboardMappingManager()
    {
        saveMappings();
    }
    
    // アクション情報の構造体
    struct ActionInfo
    {
        juce::String id;
        juce::String displayName;
    };
    
    // 全アクションのリストを取得
    static std::vector<ActionInfo> getAllActions()
    {
        std::vector<ActionInfo> actions = {
            { ACTION_REC, "REC (Record)" },
            { ACTION_PLAY, "PLAY" },
            { ACTION_UNDO, "UNDO" },
            { ACTION_TRACK_1, "Track 1 Select" },
            { ACTION_TRACK_2, "Track 2 Select" },
            { ACTION_TRACK_3, "Track 3 Select" },
            { ACTION_TRACK_4, "Track 4 Select" },
            { ACTION_TRACK_5, "Track 5 Select" },
            { ACTION_TRACK_6, "Track 6 Select" },
            { ACTION_TRACK_7, "Track 7 Select" },
            { ACTION_TRACK_8, "Track 8 Select" },
            { ACTION_AUTO_ARM, "AUTO-ARM Toggle" },
            { ACTION_VISUAL_MODE, "VISUAL MODE Toggle" },
            { ACTION_FX_MODE, "FX MODE Toggle" }
        };
        
        // FXトグルアクションを動的生成（8トラック × 6アクション = 48個）
        // これらはグリッドUIで別途表示するため、通常リストには追加しない
        // グリッド用のメソッドで取得する
        
        return actions;
    }
    
    // グリッドUI用：FXトグルアクションのリストを取得
    static std::vector<ActionInfo> getFXToggleActions()
    {
        std::vector<ActionInfo> actions;
        
        // 8トラック × 4スロット = 32個のスロットバイパスのみ
        for (int t = 1; t <= 8; ++t)
        {
            for (int s = 1; s <= 4; ++s)
            {
                juce::String id = "fx_t" + juce::String(t) + "_slot" + juce::String(s) + "_bypass";
                juce::String name = "T" + juce::String(t) + " Slot" + juce::String(s);
                actions.push_back({ id.toStdString(), name.toStdString() });
            }
        }
        
        return actions;
    }
    
    // FXアクションIDからトラックIDとスロットインデックスを抽出
    static bool parseFXActionId(const juce::String& actionId, int& trackId, int& slotIndex, juce::String& actionType)
    {
        // パターン: fx_t{trackId}_slot{slotId}_bypass または fx_t{trackId}_filter_type または fx_t{trackId}_repeat_active
        if (!actionId.startsWith("fx_t"))
            return false;
        
        // トラックID抽出
        int underscorePos = actionId.indexOf(4, "_");
        if (underscorePos < 0)
            return false;
        
        trackId = actionId.substring(4, underscorePos).getIntValue();
        if (trackId < 1 || trackId > 8)
            return false;
        
        juce::String remainder = actionId.substring(underscorePos + 1);
        
        if (remainder.startsWith("slot"))
        {
            // スロットバイパス
            int slotNum = remainder.substring(4, 5).getIntValue();
            if (slotNum >= 1 && slotNum <= 4 && remainder.endsWith("_bypass"))
            {
                slotIndex = slotNum - 1;
                actionType = "slot_bypass";
                return true;
            }
        }
        else if (remainder == "filter_type")
        {
            slotIndex = -1;
            actionType = "filter_type";
            return true;
        }
        else if (remainder == "repeat_active")
        {
            slotIndex = -1;
            actionType = "repeat_active";
            return true;
        }
        
        return false;
    }
    
    
    // キーコードからアクションIDを取得（見つからなければ空文字）
    juce::String getActionForKey(int keyCode) const
    {
        auto it = keyToAction.find(keyCode);
        if (it != keyToAction.end())
            return it->second;
        return {};
    }
    
    // アクションIDからキーコードを取得（見つからなければ-1）
    int getKeyForAction(const juce::String& actionId) const
    {
        auto it = actionToKey.find(actionId);
        if (it != actionToKey.end())
            return it->second;
        return -1;
    }
    
    // キー文字列表現を取得
    static juce::String getKeyDescription(int keyCode)
    {
        if (keyCode < 0) return "";
        
        // 特殊キー
        if (keyCode == juce::KeyPress::spaceKey) return "SPACE";
        if (keyCode == juce::KeyPress::returnKey) return "RETURN";
        if (keyCode == juce::KeyPress::escapeKey) return "ESC";
        if (keyCode == juce::KeyPress::backspaceKey) return "BACKSPACE";
        if (keyCode == juce::KeyPress::deleteKey) return "DELETE";
        if (keyCode == juce::KeyPress::tabKey) return "TAB";
        if (keyCode == juce::KeyPress::upKey) return "UP";
        if (keyCode == juce::KeyPress::downKey) return "DOWN";
        if (keyCode == juce::KeyPress::leftKey) return "LEFT";
        if (keyCode == juce::KeyPress::rightKey) return "RIGHT";
        if (keyCode >= juce::KeyPress::F1Key && keyCode <= juce::KeyPress::F12Key)
            return "F" + juce::String(keyCode - juce::KeyPress::F1Key + 1);
        
        // 通常のASCII文字のみ処理
        if (keyCode >= 'A' && keyCode <= 'Z')
            return juce::String::charToString((juce::juce_wchar)keyCode);
        if (keyCode >= 'a' && keyCode <= 'z')
            return juce::String::charToString((juce::juce_wchar)(keyCode - 32)); // 大文字に
        if (keyCode >= '0' && keyCode <= '9')
            return juce::String::charToString((juce::juce_wchar)keyCode);
        
        // その他の表示可能ASCII文字
        if (keyCode >= 33 && keyCode <= 126)
            return juce::String::charToString((juce::juce_wchar)keyCode);
        
        return "KEY " + juce::String(keyCode);
    }
    
    // マッピングを設定
    void setMapping(const juce::String& actionId, int keyCode)
    {
        // 既存のマッピングを解除
        int oldKey = getKeyForAction(actionId);
        if (oldKey >= 0)
            keyToAction.erase(oldKey);
        
        // 新しいキーが既に別のアクションに割り当てられていたら解除
        juce::String oldAction = getActionForKey(keyCode);
        if (oldAction.isNotEmpty())
            actionToKey.erase(oldAction);
        
        // 新しいマッピングを設定
        if (keyCode >= 0)
        {
            actionToKey[actionId] = keyCode;
            keyToAction[keyCode] = actionId;
        }
        else
        {
            actionToKey.erase(actionId);
        }
        
        saveMappings();
    }
    
    // マッピングを解除
    void clearMapping(const juce::String& actionId)
    {
        int key = getKeyForAction(actionId);
        if (key >= 0)
        {
            keyToAction.erase(key);
            actionToKey.erase(actionId);
            saveMappings();
        }
    }
    
private:
    std::unique_ptr<juce::PropertiesFile> propertiesFile;
    std::map<int, juce::String> keyToAction;       // keyCode -> actionId
    std::map<juce::String, int> actionToKey;       // actionId -> keyCode
    
    void loadMappings()
    {
        keyToAction.clear();
        actionToKey.clear();
        
        for (const auto& action : getAllActions())
        {
            int keyCode = propertiesFile->getIntValue("key_" + action.id, -1);
            if (keyCode >= 0)
            {
                keyToAction[keyCode] = action.id;
                actionToKey[action.id] = keyCode;
            }
        }
        
        DBG("✅ Keyboard mappings loaded");
    }
    
    void saveMappings()
    {
        for (const auto& action : getAllActions())
        {
            int keyCode = getKeyForAction(action.id);
            if (keyCode >= 0)
                propertiesFile->setValue("key_" + action.id, keyCode);
            else
                propertiesFile->removeValue("key_" + action.id);
        }
        propertiesFile->saveIfNeeded();
        DBG("🔧 Keyboard mappings saved");
    }
};
