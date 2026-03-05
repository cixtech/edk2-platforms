/** @file
  This driver implements TPM 2.0 definition block in ACPI table and
  populates registered SMI callback functions for Tcg2 physical presence
  and MemoryClear to handle the requests for ACPI method. It needs to be
  used together with Tcg2 MM drivers to exchange information on registered
  SwSmiValue and allocated NVS region address.

  Caution: This module requires additional review when modified.
  This driver will have external input - variable and ACPINvs data in SMM mode.
  This external input must be validated carefully to avoid security issue.

Copyright 2026 Cix Technology Group Co., Ltd. All Rights Reserved.
Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <IndustryStandard/Tpm2Acpi.h>

#include <Guid/TpmInstance.h>
#include <Guid/TpmNvsMm.h>
#include <Guid/PiSmmCommunicationRegionTable.h>

#include <Protocol/AcpiTable.h>
#include <Protocol/Tcg2Protocol.h>
#include <Protocol/MmCommunication.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TpmMeasurementLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/UefiLib.h>
#include <Library/MmUnblockMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Include/AcpiPlatform.h>
#include <PlatformSetupVar.h>

#define TPM_DISABLE  0
#define DTPM_ENABLE  1
#define FTPM_ENABLE  2

#ifdef EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_TREE
#else
#define EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_TREE  0x09
#endif

#ifdef EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_SPB
#else
#define EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_SPB  0x0A
#endif

STATIC UINT8  TPMDeviceSelect = 255;

EFI_TPM2_ACPI_TABLE  mTpm2AcpiTemplate = {
  {
    EFI_ACPI_5_0_TRUSTED_COMPUTING_PLATFORM_2_TABLE_SIGNATURE,
    sizeof (mTpm2AcpiTemplate),
    EFI_TPM2_ACPI_TABLE_REVISION,
    0,                          // Checksum will be updated at runtime
    EFI_ACPI_OEM_ID_PCI,        // OEMID is a 6 bytes long field
    EFI_ACPI_OEM_TABLE_ID,      // OEM table identification(8 bytes long)
    EFI_ACPI_OEM_REVISION,      // OEM revision number
    EFI_ACPI_CREATOR_ID,        // Creator vendor ID
    EFI_ACPI_CREATOR_REVISION,  // Creator revision number
  },
  0,                                                                            // Flag
  FixedPcdGet64 (PcdTpm2AcpiBufferBase),                                        // Control Area
  EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_TREE, // StartMethod
};


/**
  Publish TPM2 ACPI table

  @retval   EFI_SUCCESS     The TPM2 ACPI table is published successfully.
  @retval   Others          The TPM2 ACPI table is not published.

**/
EFI_STATUS
PublishTpm2 (
  VOID
  )
{
  EFI_STATUS               Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;
  UINTN                    TableKey;

  switch (TPMDeviceSelect) {
    case DTPM_ENABLE:
    {
      mTpm2AcpiTemplate.AddressOfControlArea = 0;
      mTpm2AcpiTemplate.StartMethod          = EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_SPB;
      break;
    }
    case FTPM_ENABLE:
    {
      mTpm2AcpiTemplate.AddressOfControlArea = FixedPcdGet64 (PcdTpm2AcpiBufferBase);
      mTpm2AcpiTemplate.StartMethod          = EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE_WITH_TREE;
      break;
    }
    default:
      return EFI_SUCCESS;
  }

  //
  // Measure to PCR[0] with event EV_POST_CODE ACPI DATA.
  // The measurement has to be done before any update.
  // Otherwise, the PCR record would be different after event log update
  // or the PCD configuration change.
  //
  TpmMeasureAndLogData (
    0,
    EV_POST_CODE,
    EV_POSTCODE_INFO_ACPI_DATA,
    ACPI_DATA_LEN,
    &mTpm2AcpiTemplate,
    mTpm2AcpiTemplate.Header.Length
    );

  //
  // Construct ACPI table
  //
  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  ASSERT_EFI_ERROR (Status);

  Status = AcpiTable->InstallAcpiTable (
                        AcpiTable,
                        &mTpm2AcpiTemplate,
                        mTpm2AcpiTemplate.Header.Length,
                        &TableKey
                        );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  The driver's entry point.

  It patches and installs ACPI tables used for handling TPM physical presence
  and Memory Clear requests through ACPI method.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval Others          Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeTcgAcpi (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS           Status = EFI_SUCCESS;
  PLATFORM_SETUP_DATA  PlatformSetupVar;
  UINTN                VarSize = sizeof (PLATFORM_SETUP_DATA);

  if (TPMDeviceSelect == 255) {
    Status = gRT->GetVariable (
                    PLATFORM_SETUP_VAR,
                    &gPlatformSetupVariableGuid,
                    NULL,
                    &VarSize,
                    &PlatformSetupVar
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a EfiGetVariable failed: %r\n", __FUNCTION__, Status));
      return Status;
    }

    TPMDeviceSelect = PlatformSetupVar.TPMDeviceSelect;
  }

  //
  // Set TPM2 ACPI table
  //
  Status = PublishTpm2 ();
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
