/*++

Copyright 2026 Cix Technology Group Co., Ltd. All Rights Reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

--*/

#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DTpmDeviceLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/Spi.h>
#include <Library/TimerLib.h>
#include <Library/Tpm2CommandLib.h>
#include <string.h>

EFI_STATUS
EFIAPI
DTpmDeviceLibConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Tpm2RegisterWrite (
  IN UINT32  registerOffset,
  IN UINT8   WriteLength,
  IN UINT32  InputData
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
Tpm2RegisterRead (
  IN UINT32  registerOffset,
  IN UINT8   ReadLength,
  OUT UINT8  *Outdata
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
Tpm2WaitRegisterBits (
  IN      UINT32  RegisterOffset,
  IN      UINT32  BitSet,
  IN      UINT32  BitClear,
  IN      UINT32  TimeOut
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
DTpmActiveLocality (
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
DTpmPrepareCommand (
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
Tpm2ReadBurstCount (
  OUT UINT32  *BurstCount
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
Tpm2WriteCommand (
  IN UINT32  CommandLen,
  IN UINT8   *CommandBlock
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
Tpm2WriteCommandWithoutLastFourBytes (
  IN UINT32  CommandLen,
  IN UINT8   *CommandBlock
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
Tpm2ReadCommandResponse (
  IN  UINT32  InResponseSize,
  OUT UINT8   *ResponseData
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
DTpmSubmitCommand (
  IN UINT32      InputParameterBlockSize,
  IN UINT8       *InputParameterBlock,
  IN OUT UINT32  *OutputParameterBlockSize,
  IN OUT UINT8   *OutputParameterBlock
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
DTpmRequestUseTpm (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
DTpmRegisterTpm2DeviceLib (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}
