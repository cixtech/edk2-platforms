/*++

Copyright (c)  1999  - 2015, Intel Corporation. All rights reserved
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

#include "DTpmFifo.h"

#define TPM2_Locality  0
#define TPM_SPI_HEADER 4

SPI_HOST_PROTOCOL  *SpiHostProtocol = NULL;
SPI_DEVICE         *SpiTpmDevice    = NULL;

/**
  The constructor function for DTpmDeviceLib.

  This constructor locates the SPI host controller that matches the configured
  SPI bus number (from PCD), opens the CixSpiHostProtocol on it, and sets up
  the SPI device for the TPM with the configured chip-select, mode and frequency.
  It caches the SPI device handle for later use by the TPM library.

  @param  VOID                This constructor takes no parameters.

  @retval EFI_SUCCESS         The SPI host was successfully located and the TPM
                              SPI device was successfully set up.
  @retval EFI_NOT_FOUND       No SPI host controller matching the configured bus
                              number was found.
  @retval EFI_ERROR           Other errors occurred during handle location,
                              protocol opening, or device setup.
**/
EFI_STATUS
EFIAPI
DTpmDeviceLibConstructor (
  VOID
  )
{
  EFI_STATUS       Status        = EFI_SUCCESS;
  UINTN            HandleCount   = 0;
  EFI_HANDLE       *HandleBuffer = NULL;
  UINT32           Index;
  SPI_DEVICE_PATH  *DevicePath;
  UINT8            SpiBus = FixedPcdGet8 (PcdDTPMSpiBus);
  UINT8            SpiCs  = FixedPcdGet8 (PcdDTPMSpiChipSelect);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gCixSpiHostProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Locate SPI Host Handle Buffer %g fail, status %r\n",
      __FUNCTION__,
      &gCixSpiHostProtocolGuid,
      Status
      ));
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    DevicePath = (SPI_DEVICE_PATH *)DevicePathFromHandle (HandleBuffer[Index]);
    if (!DevicePath) {
      continue;
    }

    if (DevicePath->Bus == SpiBus) {
      break;
    }
  }

  if (Index == HandleCount) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SPI Host [%d] is not found\n",
      __FUNCTION__,
      SpiBus
      ));
    Status = EFI_NOT_FOUND;
  } else {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gCixSpiHostProtocolGuid,
                    (VOID **)&SpiHostProtocol
                    );
    if (EFI_ERROR (Status)) {
      SpiHostProtocol = NULL;

      DEBUG ((
        DEBUG_ERROR,
        "%a: Locate SPI Host [%d] Handle Buffer %r\n",
        __FUNCTION__,
        SpiBus,
        Status
        ));
    }
  }

  if (SpiTpmDevice == NULL) {
    SpiTpmDevice = SpiHostProtocol->Setup (
                                      SpiHostProtocol,
                                      SpiCs,
                                      SPI_MODE0,
                                      5000000
                                      );
  }

  if (HandleBuffer) {
    FreePool (HandleBuffer);
  }

  return Status;
}

/*
    This function will remove the first 4 bytes of spi communication header in the output and reverse each subsequent 4 bytes
*/
VOID
EFIAPI
DataAdjust (
  OUT UINT8   *dest,
  IN  UINT8   *src,
  IN  UINT32  Size
  )
{
  UINT8  Div  = Size / 4;
  UINT8  Rest = Size % 4;
  UINTN  i    = 0;

  for ( ; i < Div - 1; i++) {
    for (UINTN j = 0; j < 4; j++) {
      dest[i * 4 + j] = *(src + (i+2) * 4 - 1 - j);
    }
  }

  if (Rest > 0) {
    for (UINTN j = 0; j < 4 && Rest > 0; j++) {
      dest[i * 4 + j] = src[ (i+2) * 4 - j];
      Rest --;
    }
  }

}

EFI_STATUS
EFIAPI
Tpm2RegisterWrite (
  IN UINT32  registerOffset,
  IN UINT8   WriteLength,
  IN UINT32  InputData
  )
{
  EFI_STATUS  Status       = TRUE;
  UINT8       Div          = WriteLength / 4;
  UINT8       Rest         = WriteLength % 4;
  UINTN       Len          = 4 + 4 * (Div + (Rest > 0 ? 1 : 0));
  UINT32      RegisterAddr = TPM2_BASE_ADDRESS + registerOffset;
  UINT8       *Txdata      = (UINT8 *)AllocateZeroPool (Len);
  UINT8       *Rxdata      = (UINT8 *)AllocateZeroPool (Len);

  for (UINTN i = 0; i < 3; i++) {
    Txdata[i] = (RegisterAddr >> (i * 8)) & 0xFF;
  }

  Txdata[3] = WriteLength-1;
  for (UINTN i = 4; i < Len; i++) {
    Txdata[i] = (InputData >> ((7-i) * 8)) & 0xFF;
  }

  SpiTpmDevice->TxBytes = Len;
  SpiTpmDevice->TxBuf   = Txdata;
  SpiTpmDevice->RxBytes = Len;
  SpiTpmDevice->RxBuf   = Rxdata;

  SpiHostProtocol->ChipSelect (SpiHostProtocol, SpiTpmDevice);
  Status = SpiHostProtocol->Transfer (SpiHostProtocol, SpiTpmDevice);
  SpiHostProtocol->ChipUnselect (SpiHostProtocol, SpiTpmDevice);
  FreePool (Txdata);
  FreePool (Rxdata);
  return Status;
}

EFI_STATUS
EFIAPI
Tpm2RegisterRead (
  IN UINT32  registerOffset,
  IN UINT8   ReadLength,
  OUT UINT8  *Outdata
  )
{
  EFI_STATUS  Status       = TRUE;
  UINT8       Div          = ReadLength / 4;
  UINT8       Rest         = ReadLength % 4;
  UINTN       Len          = 4 + 4 * (Div + (Rest > 0 ? 1 : 0));
  UINT32      RegisterAddr = TPM2_BASE_ADDRESS + registerOffset;
  UINT8       *Txdata      = (UINT8 *)AllocateZeroPool (Len);
  UINT8       *Rxdata      = (UINT8 *)AllocateZeroPool (Len);

  for (UINTN i = 0; i < 3; i++) {
    Txdata[i] = (RegisterAddr >> (i * 8)) & 0xFF;
  }

  Txdata[3] = BIT7 | (ReadLength-1);

  SpiTpmDevice->TxBytes = Len;
  SpiTpmDevice->TxBuf   = Txdata;
  SpiTpmDevice->RxBytes = Len;
  SpiTpmDevice->RxBuf   = Rxdata;

  SpiHostProtocol->ChipSelect (SpiHostProtocol, SpiTpmDevice);
  Status = SpiHostProtocol->Transfer (SpiHostProtocol, SpiTpmDevice);
  SpiHostProtocol->ChipUnselect (SpiHostProtocol, SpiTpmDevice);

  memcpy (Outdata, Rxdata, Len * sizeof (UINT8));

  FreePool (Txdata);
  FreePool (Rxdata);
  return Status;
}

/**
  Check whether the value of a TPM chip register satisfies the input BIT setting.

  @param[in]  RegisterOffset     Address port of register to be checked.
  @param[in]  BitSet       Check these data bits are set.
  @param[in]  BitClear     Check these data bits are clear.
  @param[in]  TimeOut      The max wait time (unit MicroSecond) when checking register.

  @retval     EFI_SUCCESS  The register satisfies the check bit.
  @retval     EFI_TIMEOUT  The register can't run into the expected status in time.
**/
EFI_STATUS
Tpm2WaitRegisterBits (
  IN      UINT32  RegisterOffset,
  IN      UINT32  BitSet,
  IN      UINT32  BitClear,
  IN      UINT32  TimeOut
  )
{
  UINT32  WaitTime;
  UINT8   *ResponseData = (UINT8 *)AllocateZeroPool (100);
  UINT8   DataLength    = 4;
  UINT32  RegisterData  = 0;

  for (WaitTime = 0; WaitTime < TimeOut; WaitTime += 100) {
    Tpm2RegisterRead (RegisterOffset, DataLength, ResponseData);

    RegisterData = (UINT32)(ResponseData[4] << 24) |
                   (UINT32)(ResponseData[5] << 16) |
                   (UINT32)(ResponseData[6] << 8) |
                   (UINT32)(ResponseData[7]);

    if (((RegisterData & BitSet) == BitSet) && ((RegisterData & BitClear) == 0)) {
      FreePool (ResponseData);
      return EFI_SUCCESS;
    }

    MicroSecondDelay (100);
  }

  FreePool (ResponseData);
  return EFI_TIMEOUT;
}

/**

  @retval     EFI_SUCCESS  The register satisfies the check bit.
  @retval     EFI_TIMEOUT  The register can't run into the expected status in time.
  @retval     EFI_NOT_READY  The value of register is not valid
  @retval     EFI_NOT_FOUND  The TPM is not found
**/
EFI_STATUS
EFIAPI
DTpmActiveLocality (
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT8       *ResponseData = (UINT8 *)AllocateZeroPool (100);
  UINT8       DataLength    = 1;
  UINT32      AccessData    = 0;

  Tpm2RegisterRead (TPM2_ACCESS (TPM2_Locality), DataLength, ResponseData);

  for (UINTN i = 0; i < 4; i++) {
    AccessData |= (UINT32)(ResponseData[i + 4] << ((3 - i) * 8));
  }

  if (AccessData == 0xFFFFFFFF) {
    FreePool (ResponseData);
    return EFI_NOT_FOUND;
  }

  if ((AccessData & TPM2_ACCESS_VALID) != TPM2_ACCESS_VALID) {
    FreePool (ResponseData);
    return EFI_NOT_READY;
  }

  if ((AccessData & TPM2_ACCESS_ACTIVE_LOCALITY) != TPM2_ACCESS_ACTIVE_LOCALITY) {
    Tpm2RegisterWrite (
      TPM2_ACCESS (TPM2_Locality),
      DataLength,
      TPM2_ACCESS_REQUEST_USE
      );
    Status = Tpm2WaitRegisterBits (
               TPM2_ACCESS (TPM2_Locality),
               TPM2_ACCESS_ACTIVE_LOCALITY,
               0,
               TPM2_TIMEOUT_A
               );
  }

  FreePool (ResponseData);
  return Status;
}

/**
  Set TPM chip to ready state by setting TPM2_STS_COMMAND_READY
  to Status Register in time.

  @retval    EFI_SUCCESS           TPM chip enters into ready state.
  @retval    EFI_TIMEOUT           TPM chip can't be set to ready state in time.
**/
EFI_STATUS
DTpmPrepareCommand (
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT8       *ResponseData = (UINT8 *)AllocateZeroPool (100);
  UINT8       DataLength    = 4;
  UINT32      STSData       = 0;

  Tpm2RegisterRead (TPM2_STS (TPM2_Locality), DataLength, ResponseData);
  for (UINTN i = 0; i < 4; i++) {
    STSData |= (UINT32)ResponseData[i + 4] << ((3 - i) * 8);
  }

  if ((STSData & TPM2_STS_COMMAND_READY) != TPM2_STS_COMMAND_READY) {
    Tpm2RegisterWrite (
      TPM2_STS (TPM2_Locality),
      DataLength,
      TPM2_STS_COMMAND_READY
      );
    Status = Tpm2WaitRegisterBits (
               TPM2_STS (TPM2_Locality),
               TPM2_STS_COMMAND_READY,
               0,
               TPM2_TIMEOUT_B
               );
  }

  FreePool (ResponseData);
  return Status;
}

/**
  Get BurstCount by reading the burstCount field of a TPM register

  @retval     EFI_SUCCESS             Get BurstCount.
  @retval     EFI_NOT_READY           BurstCount can't be got.
**/
EFI_STATUS
Tpm2ReadBurstCount (
  OUT UINT32  *BurstCount
  )
{
  UINT8   *ResponseData = (UINT8 *)AllocateZeroPool (100);
  UINT32  DataLength    = 4;

  for (UINTN wait = 0; wait <= TPM2_TIMEOUT_A; wait += 500) {
    *BurstCount = 0;
    Tpm2RegisterRead (TPM2_STS (TPM2_Locality), DataLength, ResponseData);
    *BurstCount |= (UINT32)ResponseData[5] << 8;
    *BurstCount |= (UINT32)ResponseData[6];
    if (*BurstCount > 0) {
      FreePool (ResponseData);
      return EFI_SUCCESS;
    }

    MicroSecondDelay (500);
  }

  FreePool (ResponseData);
  return EFI_NOT_READY;
}

EFI_STATUS
Tpm2WriteCommand (
  IN UINT32  CommandLen,
  IN UINT8   *CommandBlock
  )
{
  EFI_STATUS  Status       = EFI_SUCCESS;
  UINTN       DataFifoAddr = TPM2_BASE_ADDRESS + TPM2_DATA_FIFO (TPM2_Locality);
  UINT8       Div          = CommandLen / 4;
  UINT8       Rest         = CommandLen % 4;
  UINTN       Len          = 4 + 4 * (Div + (Rest > 0 ? 1 : 0));
  UINT8       *Txdata      = (UINT8 *)AllocateZeroPool (Len);
  UINT8       *Rxdata      = (UINT8 *)AllocateZeroPool (Len);

  for (UINTN i = 0; i < 3; i++) {
    Txdata[i] = (DataFifoAddr >> (i * 8)) & 0xFF;
  }

  Txdata[3] = CommandLen-1;
  for (UINTN i = 1; i <= Div; i++) {
    for (UINTN j = 0; j < 4; j++) {
      Txdata[i * 4 + j] = *(CommandBlock + i * 4 - 1 - j);
    }
  }

  if (Rest > 0) {
    UINTN  index = 1;
    for (UINTN j = 0; j < 4; j++) {
      if (4 - Rest++ > 0) {
        Txdata[index * 4 + j] = 0x00;
      } else {
        Txdata[index * 4 + j] = *(CommandBlock + index * 4 - 1 - j);
      }
    }
  }

  SpiTpmDevice->TxBytes = Len;
  SpiTpmDevice->TxBuf   = Txdata;
  SpiTpmDevice->RxBytes = Len;
  SpiTpmDevice->RxBuf   = Rxdata;

  SpiHostProtocol->ChipSelect (SpiHostProtocol, SpiTpmDevice);
  Status = SpiHostProtocol->Transfer (SpiHostProtocol, SpiTpmDevice);
  SpiHostProtocol->ChipUnselect (SpiHostProtocol, SpiTpmDevice);
  FreePool (Txdata);
  FreePool (Rxdata);
  return Status;
}

EFI_STATUS
Tpm2WriteCommandWithoutLastFourBytes (
  IN UINT32  CommandLen,
  IN UINT8   *CommandBlock
  )
{
  EFI_STATUS  Status           = EFI_SUCCESS;
  UINT8       AlreadyWriteSize = 0;
  UINT8       CurWriteSize     = 0;
  UINT32      *burstCount      = (UINT32 *)AllocateZeroPool (4);

  while (AlreadyWriteSize < CommandLen) {
    Status = Tpm2ReadBurstCount (burstCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: BurstCount is not ready, status %r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }

    CurWriteSize = (*burstCount) / 4 * 4;

    /* To avoid the write timeout caused by the command being too long,
     * limit the command size to 32bytes per write
    */
    if (CurWriteSize > 24) {
      CurWriteSize = 24;
    }

    if (CurWriteSize < CommandLen - AlreadyWriteSize) {
      Tpm2WriteCommand (CurWriteSize, CommandBlock + AlreadyWriteSize);
      AlreadyWriteSize += CurWriteSize;
    } else {
      Tpm2WriteCommand (CommandLen - AlreadyWriteSize, CommandBlock + AlreadyWriteSize);
      AlreadyWriteSize = CommandLen;
    }
  }

  FreePool (burstCount);
  return Status;
}

EFI_STATUS
EFIAPI
Tpm2ReadCommandResponse (
  IN  UINT32  InResponseSize,
  OUT UINT8   *ResponseData
  )
{
  EFI_STATUS  Status;
  UINT8       *OutputBlockTemp   = (UINT8 *)AllocateZeroPool (8 + InResponseSize);
  UINTN       AlreadyReadSize    = 0;
  UINT32      *burstCount        = (UINT32 *)AllocateZeroPool (4);
  UINTN       CurReadSize        = 0;
  UINTN       ResponseHeaderSize = sizeof(TPM2_RESPONSE_HEADER);
  UINT32      ResponseSize       = 0;
  TPM_RC      ResponseCode       = 0;

  //
  // Get response data header
  //
  while (AlreadyReadSize < ResponseHeaderSize) {
    Status = Tpm2ReadBurstCount (burstCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: BurstCount is not ready, status %r\n",
        __FUNCTION__,
        Status
        ));
      goto Exit;
    }
    if(*burstCount >= ResponseHeaderSize) {
      CurReadSize = ResponseHeaderSize;
    }else{
      CurReadSize = *burstCount;
    }

    Tpm2RegisterRead (TPM2_DATA_FIFO (TPM2_Locality), CurReadSize, OutputBlockTemp);
    DataAdjust (ResponseData + AlreadyReadSize, OutputBlockTemp, CurReadSize + TPM_SPI_HEADER);
    AlreadyReadSize += CurReadSize;
  }

  TPM2_RESPONSE_HEADER  *TPM_response = (TPM2_RESPONSE_HEADER *)ResponseData;

  ResponseCode = SwapBytes32 (TPM_response->responseCode);
  if (ResponseCode != TPM_RC_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "[TPM] Command Execute Fail! ResponseCode is 0x%x\n", ResponseCode));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  ResponseSize = SwapBytes32 (TPM_response->paramSize);
  if (InResponseSize < ResponseSize) {
    DEBUG ((DEBUG_ERROR, "[TPM] Response buffer is too small \n"));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // Continue reading the remaining data
  while (AlreadyReadSize < ResponseSize) {
    Status = Tpm2ReadBurstCount (burstCount);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: BurstCount is not ready, status %r\n",
        __FUNCTION__,
        Status
        ));
      goto Exit;
    }

    if(*burstCount >= ResponseSize - AlreadyReadSize) {
      CurReadSize = ResponseSize - AlreadyReadSize;
    }else{
      CurReadSize = *burstCount;
    }

    Tpm2RegisterRead (TPM2_DATA_FIFO (TPM2_Locality), CurReadSize, OutputBlockTemp);
    DataAdjust (ResponseData + AlreadyReadSize, OutputBlockTemp, CurReadSize + TPM_SPI_HEADER);
    AlreadyReadSize += CurReadSize;
  }

Exit:
  FreePool (OutputBlockTemp);
  FreePool (burstCount);
  return Status;
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
  TPM2_COMMAND_HEADER  *commandheader;

  commandheader =  (TPM2_COMMAND_HEADER *)InputParameterBlock;

  EFI_STATUS  Status      = EFI_SUCCESS;
  UINT32      *BurstCount = (UINT32 *)AllocateZeroPool (4);
  TPM_RC      ResponseCode;

  // first check if locality is active FIFO.activeLocality == 1
  Status = DTpmActiveLocality ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Locality Activation failed, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = DTpmPrepareCommand ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TPM is not Ready, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // Send command to the data fifo except the last 4 bytes
  UINT8  Div                = InputParameterBlockSize / 4;
  UINT8  Rest               = InputParameterBlockSize % 4;
  UINTN  StageOneCommandLen =  Rest > 0 ? Div*4 : (Div-1)*4;

  Tpm2WriteCommandWithoutLastFourBytes (StageOneCommandLen, InputParameterBlock);

  // check the Expect is set
  Status = Tpm2WaitRegisterBits (
             TPM2_STS (TPM2_Locality),
             TPM2_STS_VALID | TPM2_STS_DATA_EXPECT,
             0,
             TPM2_CHECK_NOW
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: The Stage 1 command receive fail, Please check your command or resent, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // Send command to the data fifo except the last 4 bytes
  Tpm2WriteCommand (InputParameterBlockSize - StageOneCommandLen, InputParameterBlock + StageOneCommandLen);
  Status = Tpm2WaitRegisterBits (
             TPM2_STS (TPM2_Locality),
             TPM2_STS_VALID,
             TPM2_STS_DATA_EXPECT,
             TPM2_TIMEOUT_C
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: The last command receive fail, Please check your commnand or resent, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // send the TPMGO to execute command
  Tpm2RegisterWrite (TPM2_STS (TPM2_Locality), 1, TPM2_STS_GO);
  Status = Tpm2WaitRegisterBits (
             TPM2_STS (TPM2_Locality),
             TPM2_STS_VALID | TPM2_STS_DATA_AVAIL,
             0,
             TPM2_TIMEOUT_A
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can not execute the command in time! status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Tpm2ReadCommandResponse (*OutputParameterBlockSize, OutputParameterBlock);
  TPM2_RESPONSE_HEADER  *TPM_response = (TPM2_RESPONSE_HEADER *)OutputParameterBlock;

  ResponseCode = SwapBytes32 (TPM_response->responseCode);
  if (ResponseCode != TPM_RC_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "[TPM] Command Execute Fail! ResponseCode is 0x%x\n", ResponseCode));
  }

  // wait the TPM execute status clear
  Status = Tpm2WaitRegisterBits (
             TPM2_STS (TPM2_Locality),
             TPM2_STS_VALID,
             TPM2_STS_DATA_AVAIL,
             TPM2_TIMEOUT_C
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TPM execute status clear fail in time, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // clear locality
  Tpm2RegisterWrite (TPM2_ACCESS (TPM2_Locality), 1, TPM2_ACCESS_ACTIVE_LOCALITY);
  Status = Tpm2WaitRegisterBits (
             TPM2_ACCESS (TPM2_Locality),
             TPM2_ACCESS_VALID,
             TPM2_ACCESS_ACTIVE_LOCALITY,
             TPM2_TIMEOUT_C
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TPM locality active clear fail in time, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  FreePool (BurstCount);
  return Status;
}

/**
  This service requests use TPM2.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
DTpmRequestUseTpm (
  VOID
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  UINT8       *ResponseData = (UINT8 *)AllocateZeroPool (100);
  UINT8       DataLength    = 4;
  UINT32      InterfaceData = 0;
  UINT32      InterfaceType = 0;

  // first check if locality is active FIFO.activeLocality == 1
  Status = DTpmActiveLocality ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Locality Activation failed, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Tpm2RegisterRead (TPM2_INTERFACE_ID (TPM2_Locality), DataLength, ResponseData);
  for (UINTN i = 0; i < 4; i++) {
    InterfaceData |= (UINT32)ResponseData[i+4] << ((3-i)*8);
  }

  InterfaceType = InterfaceData & 0x0F;
  switch (InterfaceType) {
    case TPM2_PTP_TYPE:
      DEBUG ((DEBUG_INFO, "[TPM] TPM2 PTP interface is supported\n"));
      return EFI_SUCCESS;
    case TPM2_CRB_TYPE:
      DEBUG ((DEBUG_INFO, "[TPM] TPM2 CRB interface is not supported\n"));
      return EFI_DEVICE_ERROR;
    case TPM2_TIS_TYPE:
      DEBUG ((DEBUG_INFO, "[TPM] TPM2 TIS interface is not supported\n"));
      return EFI_DEVICE_ERROR;
    default:
      DEBUG ((DEBUG_ERROR, "Unknown TPM2 interface id ! 0x%x\n", InterfaceType));
      return EFI_DEVICE_ERROR;
  }

  // clear locality
  Tpm2RegisterWrite (TPM2_ACCESS (TPM2_Locality), 1, TPM2_ACCESS_ACTIVE_LOCALITY);
  Status = Tpm2WaitRegisterBits (
             TPM2_ACCESS (TPM2_Locality),
             TPM2_ACCESS_VALID,
             TPM2_ACCESS_ACTIVE_LOCALITY,
             TPM2_TIMEOUT_C
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TPM locality active clear fail in time, status %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  return Status;
}

/**
  This service register TPM2 device.

  @Param Tpm2Device  TPM2 device

  @retval EFI_SUCCESS          This TPM2 device is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this TPM2 device.
  @retval EFI_ALREADY_STARTED  System already register this TPM2 device.
**/
EFI_STATUS
EFIAPI
DTpmRegisterTpm2DeviceLib (
  VOID
  )
{
  Tpm2Startup (TPM_SU_CLEAR);
  return EFI_UNSUPPORTED;
}
