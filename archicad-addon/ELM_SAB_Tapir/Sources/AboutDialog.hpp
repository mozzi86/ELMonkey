#pragma once

#include "DGModule.hpp"

class AboutDialog :
    public DG::ModalDialog,
    public DG::PanelObserver,
    public DG::ButtonItemObserver,
    public DG::CompoundItemObserver
{
public:
    AboutDialog ();

private:
    virtual void ButtonClicked (const DG::ButtonClickEvent& ev) override;
    void CheckForUpdate ();
    void RunSelfUpdate (const GS::UniString& latest);

    DG::Button okButton;
    DG::Button updateButton;
    DG::CenterText versionText;
};
