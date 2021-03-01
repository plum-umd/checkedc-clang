//=--3CCommands.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of 3C command helper methods.
//===----------------------------------------------------------------------===//

#ifdef INTERACTIVE3C
#include "3CCommands.h"

namespace clang {
namespace clangd {

#define _3CSOURCE "3C_RealWild"

static bool getPtrIdFromDiagMessage(const Diagnostic &DiagMsg,
                                    unsigned long &PtrId) {
  if (DiagMsg.source.rfind(_3CSOURCE, 0) == 0) {
    PtrId = atoi(DiagMsg.code.c_str());
    return true;
  }
  return false;
}

void as3CCommands(const Diagnostic &D, std::vector<Command> &OutCommands) {
  unsigned long PtrId;
  if (getPtrIdFromDiagMessage(D, PtrId)) {
    Command AllPtrsCmd;
    _3CManualFix PtrFix;
    PtrFix.PtrId = PtrId;
    AllPtrsCmd.The3CManualFix = PtrFix;
    Command SinglePtrCmd = AllPtrsCmd;

    AllPtrsCmd.command = std::string(Command::_3C_APPLY_FOR_ALL);
    AllPtrsCmd.title = "Make this pointer non-WILD and apply the "
                       "same observation to all the pointers.";

    OutCommands.push_back(AllPtrsCmd);

    SinglePtrCmd.command = std::string(Command::_3C_APPLY_ONLY_FOR_THIS);
    SinglePtrCmd.title = "Make ONLY this pointer non-WILD.";

    OutCommands.push_back(SinglePtrCmd);
  }
}

bool is3CCommand(const ExecuteCommandParams &Params) {
  return (Params.command.rfind(std::string(Command::_3C_APPLY_ONLY_FOR_THIS),
                               0) == 0) ||
         (Params.command.rfind(std::string(Command::_3C_APPLY_FOR_ALL), 0) ==
          0);
}

bool execute3CCommand(const ExecuteCommandParams &Params,
                      std::string &ReplyMessage, _3CInterface &CcInterface) {
  ReplyMessage = "Checked C Pointer Modified.";
  if (Params.command.rfind(std::string(Command::_3C_APPLY_ONLY_FOR_THIS), 0) ==
      0) {
    int PtrId = Params.The3CManualFix->PtrId;
    CcInterface.makeSinglePtrNonWild(PtrId);
    return true;
  }
  if (Params.command.rfind(std::string(Command::_3C_APPLY_FOR_ALL), 0) == 0) {
    int PtrId = Params.The3CManualFix->PtrId;
    CcInterface.invalidateWildReasonGlobally(PtrId);
    return true;
  }
  return false;
}
} // namespace clangd
} // namespace clang
#endif
