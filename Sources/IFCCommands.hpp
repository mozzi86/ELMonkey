#pragma once

#include "CommandBase.hpp"

class GetElementsByIFCIdsCommand : public CommandBase
{
public:
    GetElementsByIFCIdsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetIFCIdsOfElementsCommand : public CommandBase
{
public:
    GetIFCIdsOfElementsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetIFCTypeOfElementsCommand : public CommandBase
{
public:
    GetIFCTypeOfElementsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetIFCPropertiesOfElementsCommand : public CommandBase
{
public:
    GetIFCPropertiesOfElementsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// ELM_SAB 0.9.23 (FindElements, ifc-Blatt): IFC-Typ, -Eigenschaften und -Attribute als Text, versionsneutral.
struct ELMIFCText {
    GS::UniString propertySetName;   // leer bei Attributen
    GS::UniString name;
    GS::UniString value;
    bool          hasValue = true;
};
GSErrCode ELMGetIFCData (const API_Elem_Head& head, bool wantProperties, bool wantAttributes,
                         GS::UniString& ifcType, GS::Array<ELMIFCText>& properties, GS::Array<ELMIFCText>& attributes);
