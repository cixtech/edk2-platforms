/** @file
  Ihis library is TPM2 TREE protocol lib.

Copyright 2026 Cix Technology Group Co., Ltd. All Rights Reserved.
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/FTpmDeviceLib.h>
#include <Library/OpteeClientApiLib.h>
#include <Library/tee_client_api.h>
#include <Library/Tcg2PhysicalPresenceLib.h>

#include <Protocol/TrEEProtocol.h>
#include <Protocol/FTPMInitProtocol.h>
#include <Library/OpteeTrustedAppGuids.h>

#include <IndustryStandard/Tpm20.h>
#include <IndustryStandard/Tpm2Acpi.h>

EFI_STATUS
EFIAPI
FTpmSubmitCommand (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN UINT8       *OutputParameterBlock
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FTpmRequestUseTpm (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FTpmOpenSession (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This service indicates that it will no longer use the TPM2. Future use
  requires another call to Tpm2RequestUseTpm.
**/
VOID
EFIAPI
Tpm2RelinquishUseTpm (
  IN      EFI_EVENT  Event,
  IN      VOID       *Context
  )
{
  return;
}

/**
  This service register TPM2 device.

  @param Tpm2Device  TPM2 device

  @retval EFI_SUCCESS          This TPM2 device is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this TPM2 device.
  @retval EFI_ALREADY_STARTED  System already register this TPM2 device.
**/
EFI_STATUS
EFIAPI
FTpmRegisterTpm2DeviceLib (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FTpmDeviceLibEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_SUCCESS;
}
