#include "ParamInspectorPanel.h"
#include "GoogleTheme.h"

namespace nerou::ui::google {

ParamInspectorPanel::ParamInspectorPanel()
{
    setOpaque(false);

    presetLabel_.setText(juce::String::fromUTF8(u8"预设模板"), juce::dontSendNotification);
    presetLabel_.setFont(GoogleTheme::fontTitle(12.0f));
    presetLabel_.setColour(juce::Label::textColourId, GoogleTheme::onMuted());
    addAndMakeVisible(presetLabel_);

    emptyHint_.setText(juce::String::fromUTF8(u8"选择一个流程节点以编辑其参数"),
                       juce::dontSendNotification);
    emptyHint_.setFont(GoogleTheme::fontLabel(12.5f));
    emptyHint_.setJustificationType(juce::Justification::centred);
    emptyHint_.setColour(juce::Label::textColourId, GoogleTheme::onHint());
    addAndMakeVisible(emptyHint_);
}

ParamInspectorPanel::~ParamInspectorPanel() = default;

void ParamInspectorPanel::clear()
{
    rows_.clear();
    presetChips_.clear();
    emptyHint_.setVisible(true);
    presetLabel_.setVisible(false);
    resized();
    repaint();
}

void ParamInspectorPanel::setReadOnly(bool ro)
{
    readOnly_ = ro;
    for (auto& r : rows_)
    {
        if (r->editor) r->editor->setReadOnly(ro);
        if (r->combo)  r->combo->setEnabled(!ro);
        if (r->slider) r->slider->setEnabled(!ro);
        if (r->toggle) r->toggle->setEnabled(!ro);
    }
    for (auto& c : presetChips_)
        c->setEnabled(!ro);
}

void ParamInspectorPanel::updateValues(const juce::var& currentConfig)
{
    const auto* cfgObj = currentConfig.getDynamicObject();
    if (cfgObj == nullptr) return;
    for (auto& r : rows_)
    {
        // 跳过正在被用户编辑的 TextEditor，避免光标跳回
        if ((r->spec.kind == ParamKind::Number || r->spec.kind == ParamKind::Text)
            && r->editor && r->editor->hasKeyboardFocus(false))
            continue;

        if (cfgObj->hasProperty(r->spec.key))
            writeRowValue(*r, cfgObj->getProperty(r->spec.key));
    }
}

void ParamInspectorPanel::setSchema(const NodeSchema& schema, const juce::var& currentConfig)
{
    schema_ = schema;
    rows_.clear();
    presetChips_.clear();

    emptyHint_.setVisible(schema.fields.empty());
    presetLabel_.setVisible(!schema.presets.empty());

    rebuildPresets();
    rebuildControls(currentConfig);

    resized();
    repaint();
}

void ParamInspectorPanel::rebuildPresets()
{
    for (auto& preset : schema_.presets)
    {
        auto btn = std::make_unique<juce::TextButton>(preset.name);
        stylePresetChip(*btn);
        btn->setTooltip(preset.description);

        const ParamPreset presetCopy = preset;
        btn->onClick = [this, presetCopy]
        {
            // 把预设值写入当前控件
            for (auto& kv : presetCopy.values)
            {
                const auto key = kv.name.toString();
                for (auto& r : rows_)
                    if (r->spec.key == key)
                    {
                        writeRowValue(*r, kv.value);
                        clearError(*r);
                        if (onFieldChanged)
                            onFieldChanged(key, kv.value);
                        break;
                    }
            }
            if (onPresetSelected)
                onPresetSelected(presetCopy);
            repaint();
        };

        addAndMakeVisible(*btn);
        presetChips_.push_back(std::move(btn));
    }
}

void ParamInspectorPanel::rebuildControls(const juce::var& currentConfig)
{
    const auto* cfgObj = currentConfig.getDynamicObject();

    for (const auto& spec : schema_.fields)
    {
        auto row = std::make_unique<Row>();
        row->spec = spec;

        row->label = std::make_unique<juce::Label>();
        row->label->setText(spec.label, juce::dontSendNotification);
        row->label->setFont(GoogleTheme::fontCaption(11.5f));
        row->label->setColour(juce::Label::textColourId, GoogleTheme::onMuted());
        row->label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*row->label);

        row->hint = std::make_unique<juce::Label>();
        row->hint->setFont(GoogleTheme::fontCaption(10.5f));
        row->hint->setColour(juce::Label::textColourId, GoogleTheme::onHint());
        row->hint->setJustificationType(juce::Justification::centredLeft);
        row->hint->setText(spec.hint, juce::dontSendNotification);
        addAndMakeVisible(*row->hint);

        juce::var initial = (cfgObj != nullptr && cfgObj->hasProperty(spec.key))
                                ? cfgObj->getProperty(spec.key)
                                : spec.defaultValue;

        switch (spec.kind)
        {
            case ParamKind::Dropdown:
            {
                auto combo = std::make_unique<juce::ComboBox>();
                styleDropdown(*combo);
                for (int i = 0; i < spec.options.size(); ++i)
                    combo->addItem(spec.options[i], i + 1);

                juce::String txt = initial.toString();
                const int idx = spec.options.indexOf(txt);
                if (idx >= 0)
                    combo->setSelectedId(idx + 1, juce::dontSendNotification);
                else if (!spec.options.isEmpty())
                    combo->setSelectedId(1, juce::dontSendNotification);

                row->combo = combo.get();
                row->control = std::move(combo);
                const juce::String key = spec.key;
                row->combo->onChange = [this, row = row.get(), key] {
                    clearError(*row);
                    if (onFieldChanged)
                        onFieldChanged(key, juce::var(row->combo->getText()));
                };
                break;
            }
            case ParamKind::Slider:
            {
                auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                             juce::Slider::TextBoxRight);
                styleSlider(*slider);
                slider->setRange(spec.min, spec.max, spec.step);
                slider->setTextValueSuffix(spec.unit.isNotEmpty() ? " " + spec.unit : juce::String());
                double v = initial.isDouble() || initial.isInt() ? (double)initial
                                                                 : (double)spec.defaultValue;
                slider->setValue(v, juce::dontSendNotification);

                row->slider = slider.get();
                row->control = std::move(slider);
                const juce::String key = spec.key;
                row->slider->onValueChange = [this, row = row.get(), key] {
                    clearError(*row);
                    if (onFieldChanged)
                        onFieldChanged(key, juce::var(row->slider->getValue()));
                };
                break;
            }
            case ParamKind::Number:
            {
                auto ed = std::make_unique<juce::TextEditor>();
                styleEditor(*ed);
                ed->setInputRestrictions(0, "0123456789.-");
                ed->setText(initial.toString(), juce::dontSendNotification);
                ed->setTextToShowWhenEmpty(juce::String(spec.min, 0) + " ~ " + juce::String(spec.max, 0),
                                           GoogleTheme::onHint());
                row->editor = ed.get();
                row->control = std::move(ed);
                const juce::String key = spec.key;
                const double mn = spec.min, mx = spec.max;
                row->editor->onTextChange = [this, row = row.get(), key, mn, mx] {
                    const auto txt = row->editor->getText().trim();
                    if (txt.isEmpty())
                    {
                        clearError(*row);
                        return;
                    }
                    double v = txt.getDoubleValue();
                    if (v < mn || v > mx)
                        markError(*row, juce::String::fromUTF8(u8"超出范围 ")
                                            + juce::String(mn) + " ~ " + juce::String(mx));
                    else
                    {
                        clearError(*row);
                        if (onFieldChanged)
                            onFieldChanged(key, juce::var(v));
                    }
                };
                break;
            }
            case ParamKind::Toggle:
            {
                auto tb = std::make_unique<juce::ToggleButton>(spec.label);
                styleToggle(*tb);
                tb->setToggleState((bool)initial, juce::dontSendNotification);
                row->toggle = tb.get();
                row->control = std::move(tb);
                const juce::String key = spec.key;
                row->toggle->onClick = [this, row = row.get(), key] {
                    clearError(*row);
                    if (onFieldChanged)
                        onFieldChanged(key, juce::var(row->toggle->getToggleState()));
                };
                row->label->setVisible(false); // toggle 已显示 label 文字
                break;
            }
            case ParamKind::Text:
            default:
            {
                auto ed = std::make_unique<juce::TextEditor>();
                styleEditor(*ed);
                ed->setText(initial.toString(), juce::dontSendNotification);
                row->editor = ed.get();
                row->control = std::move(ed);
                const juce::String key = spec.key;
                row->editor->onTextChange = [this, row = row.get(), key] {
                    clearError(*row);
                    if (onFieldChanged)
                        onFieldChanged(key, juce::var(row->editor->getText()));
                };
                break;
            }
        }

        addAndMakeVisible(*row->control);
        rows_.push_back(std::move(row));
    }
}

juce::var ParamInspectorPanel::readRowValue(const Row& row) const
{
    switch (row.spec.kind)
    {
        case ParamKind::Dropdown: return row.combo  ? juce::var(row.combo->getText()) : juce::var();
        case ParamKind::Slider:   return row.slider ? juce::var(row.slider->getValue()) : juce::var();
        case ParamKind::Number:   return row.editor ? juce::var(row.editor->getText().getDoubleValue()) : juce::var();
        case ParamKind::Toggle:   return row.toggle ? juce::var(row.toggle->getToggleState()) : juce::var();
        case ParamKind::Text:
        default:                  return row.editor ? juce::var(row.editor->getText()) : juce::var();
    }
}

void ParamInspectorPanel::writeRowValue(Row& row, const juce::var& v)
{
    switch (row.spec.kind)
    {
        case ParamKind::Dropdown:
            if (row.combo)
            {
                const auto txt = v.toString();
                const int idx = row.spec.options.indexOf(txt);
                if (idx >= 0)
                    row.combo->setSelectedId(idx + 1, juce::dontSendNotification);
            }
            break;
        case ParamKind::Slider:
            if (row.slider)
                row.slider->setValue(v.isDouble() || v.isInt() ? (double)v : row.slider->getValue(),
                                     juce::dontSendNotification);
            break;
        case ParamKind::Number:
        case ParamKind::Text:
            if (row.editor)
                row.editor->setText(v.toString(), juce::dontSendNotification);
            break;
        case ParamKind::Toggle:
            if (row.toggle)
                row.toggle->setToggleState((bool)v, juce::dontSendNotification);
            break;
    }
}

bool ParamInspectorPanel::collectValues(juce::DynamicObject& out, juce::String& error)
{
    for (auto& r : rows_)
    {
        const auto v = readRowValue(*r);

        if (r->spec.kind == ParamKind::Number && r->editor != nullptr)
        {
            const auto raw = r->editor->getText().trim();
            if (raw.isEmpty()) continue;
            double d = raw.getDoubleValue();
            if (d < r->spec.min || d > r->spec.max)
            {
                markError(*r, juce::String::fromUTF8(u8"超出范围"));
                error = r->spec.label + juce::String::fromUTF8(u8" 超出范围");
                return false;
            }
            out.setProperty(r->spec.key, juce::var(d));
            continue;
        }
        if (r->spec.kind == ParamKind::Slider && r->slider != nullptr)
        {
            const double d = r->slider->getValue();
            if (d < r->spec.min || d > r->spec.max)
            {
                markError(*r, juce::String::fromUTF8(u8"超出滑条范围"));
                error = r->spec.label + juce::String::fromUTF8(u8" 超出滑条范围");
                return false;
            }
            out.setProperty(r->spec.key, juce::var(d));
            continue;
        }

        out.setProperty(r->spec.key, v);
    }
    return true;
}

void ParamInspectorPanel::markError(Row& row, const juce::String& msg)
{
    row.hasError = true;
    row.errorMessage = msg;
    if (row.editor)
    {
        row.editor->setColour(juce::TextEditor::outlineColourId, GoogleTheme::danger());
        row.editor->setColour(juce::TextEditor::focusedOutlineColourId, GoogleTheme::danger());
    }
    if (row.hint)
    {
        row.hint->setText(msg, juce::dontSendNotification);
        row.hint->setColour(juce::Label::textColourId, GoogleTheme::danger());
    }
    repaint();
}

void ParamInspectorPanel::clearError(Row& row)
{
    if (!row.hasError) return;
    row.hasError = false;
    row.errorMessage.clear();
    if (row.editor)
    {
        row.editor->setColour(juce::TextEditor::outlineColourId, GoogleTheme::outlineSoft());
        row.editor->setColour(juce::TextEditor::focusedOutlineColourId, GoogleTheme::blue());
    }
    if (row.hint)
    {
        row.hint->setText(row.spec.hint, juce::dontSendNotification);
        row.hint->setColour(juce::Label::textColourId, GoogleTheme::onHint());
    }
    repaint();
}

// ---- styling ----

void ParamInspectorPanel::stylePresetChip(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId,   GoogleTheme::blueTint());
    b.setColour(juce::TextButton::buttonOnColourId, GoogleTheme::blue());
    b.setColour(juce::TextButton::textColourOffId,  GoogleTheme::bluePress());
    b.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
}

void ParamInspectorPanel::styleDropdown(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId,       GoogleTheme::surface());
    c.setColour(juce::ComboBox::textColourId,             GoogleTheme::onSurface());
    c.setColour(juce::ComboBox::outlineColourId,          GoogleTheme::outlineSoft());
    c.setColour(juce::ComboBox::focusedOutlineColourId,   GoogleTheme::blue());
    c.setColour(juce::ComboBox::arrowColourId,            GoogleTheme::onMuted());
    c.setColour(juce::ComboBox::buttonColourId,           GoogleTheme::surface());
}

void ParamInspectorPanel::styleEditor(juce::TextEditor& e)
{
    e.setFont(GoogleTheme::fontLabel(12.5f));
    e.setColour(juce::TextEditor::backgroundColourId, GoogleTheme::surface());
    e.setColour(juce::TextEditor::textColourId, GoogleTheme::onSurface());
    e.setColour(juce::TextEditor::outlineColourId, GoogleTheme::outlineSoft());
    e.setColour(juce::TextEditor::focusedOutlineColourId, GoogleTheme::blue());
    e.setColour(juce::CaretComponent::caretColourId, GoogleTheme::blue());
    e.setBorder({ 6, 10, 6, 10 });
}

void ParamInspectorPanel::styleSlider(juce::Slider& s)
{
    s.setColour(juce::Slider::backgroundColourId,    GoogleTheme::outlineSoft());
    s.setColour(juce::Slider::trackColourId,         GoogleTheme::blue());
    s.setColour(juce::Slider::thumbColourId,         GoogleTheme::blue());
    s.setColour(juce::Slider::textBoxBackgroundColourId, GoogleTheme::surface());
    s.setColour(juce::Slider::textBoxTextColourId,   GoogleTheme::onSurface());
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
}

void ParamInspectorPanel::styleToggle(juce::ToggleButton& t)
{
    t.setColour(juce::ToggleButton::textColourId,             GoogleTheme::onSurface());
    t.setColour(juce::ToggleButton::tickColourId,             GoogleTheme::blue());
    t.setColour(juce::ToggleButton::tickDisabledColourId,     GoogleTheme::outline());
}

int ParamInspectorPanel::rowHeightFor(const Row& row) const
{
    switch (row.spec.kind)
    {
        case ParamKind::Slider:   return 58; // label + slider
        case ParamKind::Dropdown: return 56;
        case ParamKind::Number:
        case ParamKind::Text:     return 56;
        case ParamKind::Toggle:   return 34;
    }
    return 50;
}

int ParamInspectorPanel::getPreferredHeight() const
{
    int h = 0;
    if (!schema_.presets.empty())
        h += 16 + 28 + 8; // preset label + chip row + gap
    for (auto& r : rows_)
        h += rowHeightFor(*r) + 6;
    return juce::jmax(h, 60);
}

void ParamInspectorPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void ParamInspectorPanel::resized()
{
    auto area = getLocalBounds();
    if (schema_.fields.empty() && schema_.presets.empty())
    {
        emptyHint_.setBounds(area);
        return;
    }
    emptyHint_.setVisible(false);

    // ---- Preset chips row ----
    if (!schema_.presets.empty())
    {
        presetLabel_.setVisible(true);
        presetLabel_.setBounds(area.removeFromTop(16));
        auto chipRow = area.removeFromTop(32);
        int x = chipRow.getX();
        const int chipH = 28;
        const int chipGap = 6;
        for (auto& c : presetChips_)
        {
            auto txtWidth = juce::Font(GoogleTheme::fontLabel(12.0f)).getStringWidth(c->getButtonText());
            const int chipW = juce::jmin(chipRow.getRight() - x, txtWidth + 28);
            c->setBounds(x, chipRow.getY() + (chipRow.getHeight() - chipH) / 2, chipW, chipH);
            x += chipW + chipGap;
        }
        area.removeFromTop(8);
    }
    else
    {
        presetLabel_.setVisible(false);
    }

    // ---- Rows ----
    for (auto& r : rows_)
    {
        const int h = rowHeightFor(*r);
        auto row = area.removeFromTop(h);
        area.removeFromTop(4);

        if (r->spec.kind == ParamKind::Toggle)
        {
            r->control->setBounds(row.removeFromTop(26));
            r->hint->setBounds(row);
            continue;
        }

        r->label->setBounds(row.removeFromTop(16));
        auto ctrlArea = row.removeFromTop(28);
        r->control->setBounds(ctrlArea);
        r->hint->setBounds(row);
    }
}

} // namespace nerou::ui::google
