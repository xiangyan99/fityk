// This file is part of fityk program. Copyright 2001-2013 Marcin Wojdyr
// Licence: GNU General Public License ver. 2+
///  Definition Manager Dialog (DefinitionMgrDlg)

#include <wx/wx.h>
#include <wx/statline.h>
#include <wx/hyperlink.h>

#include "defmgr.h"
#include "cmn.h" //s2wx, wx2s
#include "frame.h" // ftk
#include "app.h" // get_help_url()
#include "fityk/logic.h"
#include "fityk/func.h"
#include "fityk/lexer.h"
#include "fityk/udf.h"
#include "fityk/guess.h" //Guess::Kind

using namespace std;
using fityk::Tplate;

DefinitionMgrDlg::DefinitionMgrDlg(wxWindow* parent)
    : wxDialog(parent, -1, wxT("Function Definition Manager"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER),
      selected_(-1), parser_(ftk)
{
    wxBoxSizer *top_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *lb_sizer = new wxBoxSizer(wxVERTICAL);
    lb = new wxListBox(this, -1, wxDefaultPosition, wxDefaultSize,
                       0, 0, wxLB_SINGLE);
    lb_sizer->Add(lb, 1, wxEXPAND|wxALL, 5);
    wxBoxSizer *ar_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* add_btn = new wxButton(this, wxID_ADD);
    ar_sizer->Add(add_btn, 0, wxALL|wxALIGN_CENTER, 5);
    remove_btn = new wxButton(this, wxID_REMOVE);
    ar_sizer->Add(remove_btn, 0, wxALL|wxALIGN_CENTER, 5);
    lb_sizer->Add(ar_sizer, 0, wxEXPAND);
    hsizer->Add(lb_sizer, 0, wxEXPAND);

    wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

    def_label_st = new wxStaticText(this, -1, wxT("definition:"),
                    wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
    vsizer->Add(def_label_st, 0, wxEXPAND|wxALL, 5);
    def_tc = new wxTextCtrl(this, -1, wxT(""),
                            wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    vsizer->Add(def_tc, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 5);

    desc_tc = new wxTextCtrl(this, -1, wxT(""),
               wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
    wxBoxSizer *link_sizer = new wxBoxSizer(wxHORIZONTAL);
    link_sizer->Add(new wxStaticText(this, -1, wxT("Description:")), 0);
    link_sizer->AddStretchSpacer();
    base_url_ = get_help_url("model.html");
    link = new wxHyperlinkCtrl(this, -1, "documentation", base_url_);
    link_sizer->Add(link, 0);
    vsizer->Add(link_sizer, 0, wxALL|wxEXPAND, 5);
    vsizer->Add(desc_tc, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 5);
    desc_tc->SetBackgroundColour(GetBackgroundColour());

    hsizer->Add(vsizer, 1, wxEXPAND);
    top_sizer->Add(hsizer, 1, wxEXPAND);

    top_sizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 5);
    top_sizer->Add(CreateButtonSizer (wxOK|wxCANCEL),
                   0, wxALL|wxALIGN_CENTER, 5);

    SetSizerAndFit(top_sizer);
    SetSize(560, 512);

    // fill functions list
    lb->Clear();
    modified_.clear();
    modified_.reserve(ftk->get_tpm()->tpvec().size());
    v_foreach (Tplate::Ptr, i, ftk->get_tpm()->tpvec()) {
        lb->Append(s2wx((*i)->name));
        modified_.push_back(**i);
    }

    Connect(def_tc->GetId(), wxEVT_COMMAND_TEXT_UPDATED,
            wxCommandEventHandler(DefinitionMgrDlg::OnDefChanged));
    Connect(wxID_ADD, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(DefinitionMgrDlg::OnAddButton));
    Connect(wxID_REMOVE, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(DefinitionMgrDlg::OnRemoveButton));
    Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(DefinitionMgrDlg::OnOk));
    Connect(lb->GetId(), wxEVT_COMMAND_LISTBOX_SELECTED,
            wxCommandEventHandler(DefinitionMgrDlg::OnFunctionChanged));

    ok_btn = (wxButton*) FindWindow(wxID_OK);
    lb->SetSelection(0);
    select_function();
}

void DefinitionMgrDlg::parse_definition()
{
    assert(selected_ >= 0 && selected_ < (int) modified_.size());
    assert(modified_.size() == lb->GetCount());
    Tplate& tp = modified_[selected_];
    if (tp.is_coded())
        return;
    string value = wx2s(def_tc->GetValue().Trim());
    if (value.empty()) {
        desc_tc->Clear();
        if (lb->GetString(selected_) != wxT("-"))
            lb->SetString(selected_, wxT("-"));
        ok_btn->Enable(false);
        return;
    }
    try {
        fityk::Lexer lex(value.c_str());
        tp = *parser_.parse_define_args(lex);
        update_desc(tp);
        // We check if SetString() is needed, because on wxGTK 2.8
        // the program crashed when pressing PgDown or PgUp in the listbox,
        // and these checks helped to avoid crashes. Now parse_definition()
        // is not called from the listbox selection event, so it should not
        // matter, but the checks are left just in case.
        if (lb->GetString(selected_) != s2wx(tp.name))
            lb->SetString(selected_, s2wx(tp.name));
    }
    catch (exception &e) {
        desc_tc->ChangeValue(pchar2wx(e.what()));
        if (lb->GetString(selected_) != wxT("-"))
            lb->SetString(selected_, wxT("-"));
    }

    bool all_ok = (lb->FindString(wxT("-")) == wxNOT_FOUND);
    ok_btn->Enable(all_ok);
}

void DefinitionMgrDlg::update_desc(const Tplate& tp)
{
    wxString desc = wxString::Format(wxT("%d args:"), (int)tp.fargs.size());
    v_foreach (string, i, tp.fargs)
        desc += wxT(" ") + s2wx(*i);

    desc += "\ntraits:";
    bool has_traits = false;
    if (tp.traits & Tplate::kLinear) {
        desc += " linear";
        has_traits = true;
    }
    if (tp.traits & Tplate::kPeak) {
        if (has_traits)
            desc += " +";
        desc += " peak";
        has_traits = true;
    }
    if (tp.traits & Tplate::kSigmoid) {
        if (has_traits)
            desc += " +";
        desc += " sigmoid";
        has_traits = true;
    }
    if (!has_traits)
        desc += " none";

    desc += "\nused by:";
    bool used = false;
    v_foreach (Tplate::Ptr, i, ftk->get_tpm()->tpvec())
        v_foreach (Tplate::Component, c, (*i)->components)
            if (c->p && c->p->name == tp.name) {
                desc += wxT(" ") + s2wx((*i)->name);
                used = true;
                break; // don't report the same tplate twice
            }
    v_foreach (fityk::Function*, i, ftk->mgr.functions())
        if ((*i)->tp()->name == tp.name) {
            desc += " %" + s2wx((*i)->name);
            used = true;
        }
    if (!used)
        desc += wxT(" -");

    desc_tc->ChangeValue(desc);
}

void DefinitionMgrDlg::select_function()
{
    int n = lb->GetSelection();
    if (n == selected_)
        return;
    if (n == wxNOT_FOUND) {
        lb->SetSelection(selected_);
        return;
    }
    if (selected_ != wxNOT_FOUND) {
        const Tplate& old = modified_[selected_];
        wxString name = old.rhs.empty() ? wxString(wxT("-")) : s2wx(old.name);
        if (lb->GetString(selected_) != name)
            lb->SetString(selected_, name);
    }

    selected_ = n;
    const Tplate& tp = modified_[n];
    Tplate::Ptr orig_ptr = ftk->get_tpm()->get_shared_tp(tp.name);
    // minimal use_count() is 2: this pointer and the one in TplateMgr::tpvec_
    bool used = (orig_ptr.use_count() > 2);

    def_tc->ChangeValue(s2wx(tp.as_formula()));
    def_tc->SetEditable(!tp.is_coded() && !used);
    def_label_st->SetLabel(tp.is_coded() ? wxT("definition (equivalent):")
                                         : wxT("definition:"));
    remove_btn->Enable(!used);
    link->Enable(tp.docs_fragment != NULL);
    if (tp.docs_fragment != NULL)
    // wxMSW converts URI back to filename, and it can't handle #fragment
#ifndef __WXMSW__
        link->SetURL(base_url_ + "#" + tp.docs_fragment);
#else
        link->SetURL(base_url_);
#endif
    else
        link->SetURL("no docs for " + s2wx(tp.name));
    update_desc(tp);
}

vector<string> DefinitionMgrDlg::get_commands()
{
    vector<string> ss;

    v_foreach (Tplate::Ptr, i, ftk->get_tpm()->tpvec()) {
        bool found = false;
        v_foreach (Tplate, j, modified_) {
            if ((*i)->name == j->name) {
                found = true;
                break;
            }
        }
        if (!found)
            ss.push_back("undefine " + (*i)->name);
    }

    v_foreach (Tplate, i, modified_) {
        bool need_define = true;
        v_foreach (Tplate::Ptr, j, ftk->get_tpm()->tpvec()) {
            if (i->name == (*j)->name) {
                if (i->fargs == (*j)->fargs && i->defvals == (*j)->defvals &&
                        i->rhs == (*j)->rhs)
                    need_define = false;
                else
                    ss.push_back("undefine " + i->name);
                break;
            }
        }
        if (need_define)
            ss.push_back("define " + i->as_formula());
    }
    return ss;
}

void DefinitionMgrDlg::OnAddButton(wxCommandEvent &)
{
    Tplate tp;
    tp.traits = 0;
    tp.create = NULL;
    tp.docs_fragment = NULL;
    modified_.push_back(tp);
    lb->Append(wxT("-"));
    lb->SetSelection(lb->GetCount() - 1);
    select_function();
    def_tc->SetFocus();
}


void DefinitionMgrDlg::OnRemoveButton(wxCommandEvent &)
{
    if (!is_index(selected_, modified_))
        return;
    modified_.erase(modified_.begin() + selected_);
    lb->Delete(selected_);
    if (modified_.empty())
        return;
    lb->SetSelection(selected_ > 0 ? selected_ - 1 : 0);
    selected_ = -1;
    select_function();
}

void DefinitionMgrDlg::OnOk(wxCommandEvent&)
{
    if (lb->FindString(wxT("new")) != wxNOT_FOUND)
        return;
    EndModal(wxID_OK);
}

