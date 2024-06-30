#pragma once
#include <pxr/usd/usd/prim.h>

//
// Prototype of a connection editor working at the stage level
//
PXR_NAMESPACE_USING_DIRECTIVE

void DrawConnectionEditor(const UsdStageRefPtr& prim);

// Experimental; testing several ways to bring prims in the connection editor
void CreateSession(const UsdPrim &prim, const std::vector<UsdPrim> &prims);
void AddPrimsToCurrentSession(const std::vector<UsdPrim> &prims);
