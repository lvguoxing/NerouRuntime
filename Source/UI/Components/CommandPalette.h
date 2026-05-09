#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace nerou::ui {

class CommandPalette : public juce::Component,
                       private juce::ListBoxModel,
                       private juce::TextEditor::Listener
{
public:
    struct CommandItem
    {
        juce::String id;
        juce::String label;
        juce::StringArray aliases;
        juce::String tooltip;
        bool enabled = true;
    };

    CommandPalette();
    ~CommandPalette() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setVisiblePalette(bool on);
    bool isVisiblePalette() const noexcept { return isVisible(); }

    void setCommands(std::vector<CommandItem> commands);
    const std::vector<CommandItem>& getCommands() const noexcept { return all_; }
    void recordCommand(const juce::String& cmd);
    std::function<void(const CommandItem&)> onCommandTriggered;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& t) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;

    void rebuildFiltered();
    void triggerRow(int row);

    juce::TextEditor input_;
    juce::ListBox    list_ { "cmd_list", this };
    std::vector<CommandItem> all_;
    std::vector<int> filtered_;
    juce::StringArray recent_;
};

} // namespace nerou::ui

