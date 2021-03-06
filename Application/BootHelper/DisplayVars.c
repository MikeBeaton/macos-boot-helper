/** @file
  NVRAM var list and display methods.

  Copyright (c) 2020, Mike Beaton. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-3-Clause

**/

//
// Basic UEFI Libraries
//
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

//
// Boot and Runtime Services
//
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

//
// OC Libraries
//
#include <Library/OcDebugLogLib.h>

//
// Local includes
//
#include "BootHelper.h"
#include "EzKb.h"
#include "Utils.h"

#define EFI_QEMU_C16_GUID_1 \
  { 0x158DEF5A, 0xF656, 0x419C, {0xB0, 0x27, 0x7A, 0x31, 0x92, 0xC0, 0x79, 0xD2} }
#define EFI_QEMU_C16_GUID_2 \
  { 0x0053D9D6, 0x2659, 0x4599, {0xA2, 0x6B, 0xEF, 0x45, 0x36, 0xE6, 0x31, 0xA9} }

STATIC EFI_GUID gEfiQemuC16lGuid1 = EFI_QEMU_C16_GUID_1;
STATIC EFI_GUID gEfiQemuC16lGuid2 = EFI_QEMU_C16_GUID_2;

CHAR16 HexChar(
  CHAR16 c
  )
{
  if (c < 10) return c + L'0';
  else return c - 10 + L'a';
}

// Display NVRAM var as a CHAR8 string
VOID
DisplayVarC8 (
  IN CHAR8    *Data,
  UINTN       CharSize,
  BOOLEAN     isString
  )
{
  Print (L"\"");
  for (UINTN i = 0; i < CharSize; i++) {
    CHAR8 c = Data[i];
    if (isString && c >= 32 && c < 127) {
      Print (L"%c", (CHAR16)c);
      if (c == '%') Print(L"%%"); // escape % so that representation is unambiguous & reversible
    } else {
      Print (L"%%");
      Print (L"%c", HexChar((Data[i] >> 4) & 0xF));
      Print (L"%c", HexChar(Data[i] & 0xF));
    }
  }
  Print (L"\"");

  if (CharSize == 8) {
    Print (L" 0x%016lx", ((UINT64*)Data)[0]);
  } else if (CharSize == 4) {
    Print (L" 0x%08x", ((UINT32*)Data)[0]);
  } else if (CharSize == 2) {
    Print (L" 0x%04x", ((UINT16*)Data)[0]);
  } else if (CharSize == 1) {
    Print (L" 0x%02x", ((UINT8*)Data)[0]);
  }
}

// Display NVRAM var as a CHAR16 string
VOID
DisplayVarC16 (
  IN CHAR16*  Data,
  UINTN       CharSize,
  BOOLEAN     isString
  )
{
  Print (L"L\"");
  for (UINTN i = 0; i < CharSize; i++) {
    CHAR16 c = Data[i];
    if (isString && c >= 32) {
      Print (L"%c", c);
      if (c == L'%') Print (L"%%"); // escape % so that representation is unambiguous & reversible
    } else {
      Print (L"%%");
      Print (L"%c", HexChar((Data[i] >> 12) & 0xF));
      Print (L"%c", HexChar((Data[i] >> 8) & 0xF));
      Print (L"%c", HexChar((Data[i] >> 4) & 0xF));
      Print (L"%c", HexChar(Data[i] & 0xF));
    }
  }
  Print (L"\"");
}

// Display NVRAM var, automatically deciding (based on GUID) whether it is likely to be a CHAR8 or CHAR16 string
// (Only some QEMU vars are currently displayed as CHAR16, but it is nice to be able to read the relevant strings easily.)
VOID
DisplayVar (
  IN EFI_GUID     *Guid,
  IN VOID         *Data,
  UINTN           DataSize,
  BOOLEAN         isString
  )
{
  // some known guid's which seem to have only CHAR16 strings Data them
  // don't even try to display it as CHAR16 string if byte size is odd
  if ((DataSize & 1) == 0 && (
    CompareMem (Guid, &gEfiQemuC16lGuid1, sizeof(EFI_GUID)) == 0 ||
    CompareMem (Guid, &gEfiQemuC16lGuid2, sizeof(EFI_GUID)) == 0
  )) {
    DisplayVarC16 ((CHAR16 *)Data, DataSize >> 1, isString);
  } else {
    DisplayVarC8 ((CHAR8 *)Data, DataSize, isString);
  }
}

EFI_STATUS
GetNvramValue (
  IN CHAR16     *Name,
  IN EFI_GUID   *Guid,
  OUT UINT32    *Attr,
  OUT UINTN     *DataSize,
  OUT VOID      **Data)
{
  EFI_STATUS Status;

    //
    // Initialize variable data buffer as an empty buffer
    //
    *DataSize = 0;
    *Data = NULL;

    //
    // Loop until a large enough variable data buffer is allocated
    //
    do {
        Status = gRT->GetVariable(Name, Guid, Attr, DataSize, *Data);
        if (Status == EFI_BUFFER_TOO_SMALL) {
            //
            // Allocate new buffer for the variable data
            //
            *Data = AllocatePool(*DataSize);
            if (*Data == NULL) {
                return EFI_OUT_OF_RESOURCES;
            }
        }
    } while (Status == EFI_BUFFER_TOO_SMALL);

    if (EFI_ERROR(Status)) {
    if (*Data == NULL) {
      ASSERT (Status == EFI_NOT_FOUND);
    } else {
      FreePool(*Data);
    }
        return Status;
    }

  return EFI_SUCCESS;
}

EFI_STATUS
DisplayNvramValueOptionalGuid (
  IN CHAR16       *Name,
  IN EFI_GUID     *Guid,
  BOOLEAN         isString,
  BOOLEAN         displayGuid
  )
{
  EFI_STATUS Status;
  UINT32 Attributes;
  UINTN DataSize;
  VOID *Data;

  if (displayGuid) {
      Print(L"%g:", Name);
  }

    Print(L"%s", Name);

    Status = GetNvramValue(Name, Guid, &Attributes, &DataSize, &Data);

    if (EFI_ERROR(Status)) {
    if (Status == EFI_NOT_FOUND) {
      Print(L": EFI_NOT_FOUND\n");
    } else {
      Print(L": EFI_UNKOWN_STATUS=%0x\n", Status);
    }

        return Status;
    }

    Print(L" = ");
    DisplayVar(Guid, Data, DataSize, TRUE);
  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
    Print(L" (non-persistent)");
  }
    Print(L"\n");

    FreePool(Data);

  return EFI_SUCCESS;
}

EFI_STATUS
DisplayNvramValue (
  IN CHAR16     *Name,
  IN EFI_GUID   *Guid,
  BOOLEAN       isString
  )
{
  return DisplayNvramValueOptionalGuid (Name, Guid, isString, TRUE);
}

EFI_STATUS
DisplayNvramValueWithoutGuid (
  IN CHAR16     *Name,
  IN EFI_GUID   *Guid,
  BOOLEAN       isString
  )
{
  return DisplayNvramValueOptionalGuid (Name, Guid, isString, FALSE);
}

EFI_STATUS
ListVars ()
{
  EFI_STATUS  Status;
  EFI_GUID    Guid;
  UINTN       NameBufferSize;
  UINTN       NameSize;
  CHAR16      *Name;

  BOOLEAN     showAll = FALSE;

  //
  // Initialize the variable name and data buffer variables
  // to retrieve the first variable name Data the variable store
  //
  NameBufferSize = sizeof(CHAR16);
  Name = AllocateZeroPool(NameBufferSize);

  //
  // Loop through all variables Data the variable store
  //
  while (TRUE)
  {
    do {
      //
      // Loop until a large enough variable name buffer is allocated
      // do {
      NameSize = NameBufferSize;
      Status = gRT->GetNextVariableName(&NameSize, Name, &Guid);
      if (Status == EFI_BUFFER_TOO_SMALL) {
        //
        // Grow the buffer Name to NameSize bytes
        //
        Name = ReallocatePool(NameBufferSize, NameSize, Name);
        if (Name == NULL) {
          return EFI_OUT_OF_RESOURCES;
        }
        NameBufferSize = NameSize;
      }
    } while (Status == EFI_BUFFER_TOO_SMALL);

    //
    // Exit main loop after last variable name is retrieved
    //
    if (EFI_ERROR (Status)) {
      FreePool (Name);
      return Status;
    }

    //
    // Display var
    //
        Status = DisplayNvramValue (Name, &Guid, TRUE);

    //
    // Not expecting error here but exit if there is one
    //
    if (EFI_ERROR (Status)) {
      FreePool (Name);
      return Status;
    }

    //
    // Keyboard control
    //
    if (!showAll) {
      EFI_INPUT_KEY key;
      getkeystroke (&key);

      CHAR16 c = key.UnicodeChar;
      if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
      if (c == 'q' || c == 'x') {
        FreePool (Name);
        return EFI_SUCCESS;
      } else if (c == 'a') {
        showAll = TRUE;
      }
    }
  }
}

STATIC UINT32 gFlags = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE;

EFI_STATUS
ToggleOrSetVar(
  IN CHAR16     *Name,
  IN EFI_GUID   *Guid,
  IN CHAR8      *PreferredValue,
  UINTN         PreferredSize,
  BOOLEAN       Toggle
  )
{
  EFI_STATUS Status;
  UINT32 Attr;
  UINTN DataSize;
  VOID *Data;

  Status = GetNvramValue (Name, Guid, &Attr, &DataSize, &Data);

  if (Status != EFI_NOT_FOUND && EFI_ERROR (Status)) {
    return Status;
  }

  if (!mInteractive) SetColour(EFI_LIGHTGREEN);

  if (Status != EFI_NOT_FOUND &&
      DataSize == PreferredSize &&
    CompareMem (PreferredValue, Data, DataSize) == 0)
  {
    if (Toggle)
    {
      gRT->SetVariable (Name, Guid, gFlags, 0, NULL);
      if (!mInteractive) Print(L"Deleting %s\n", Name);
    }
    else
    {
      if (!mInteractive) Print(L"Not setting %s, already set\n", Name);
    }
  }
  else
  {
    gRT->SetVariable (Name, Guid, gFlags, PreferredSize, PreferredValue);
    if (!mInteractive) Print(L"Setting %s\n", Name);
  }

  if (Status != EFI_NOT_FOUND) {
    FreePool (Data);
  }

  if (!mInteractive) SetColour(EFI_WHITE);

  return EFI_SUCCESS;
}
