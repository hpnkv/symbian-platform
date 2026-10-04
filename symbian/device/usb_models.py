"""Typed results from the native libusb transport and protocol probes."""

from pydantic import BaseModel, ConfigDict, Field


def _absent(value: object) -> bool:
    """Omit optional fields whose values were not observed."""
    return value is None


def _empty(value: object) -> bool:
    """Omit empty optional collections from serialized probe output."""
    return not value


class UsbModel(BaseModel):
    """Validate native structs by their declared attributes."""

    model_config = ConfigDict(from_attributes=True, frozen=True)


class UsbEndpointDescriptor(UsbModel):
    """One endpoint in a USB interface alternate setting."""

    address: int = Field(description="Endpoint address including direction bit")
    direction: str = Field(description="Transfer direction, in or out")
    transfer: str = Field(description="USB transfer type")
    max_packet_bytes: int = Field(description="Maximum endpoint packet size")


class UsbInterfaceDescriptor(UsbModel):
    """USB interface and its endpoints."""

    number: int = Field(description="Interface number")
    alternate_setting: int = Field(description="Alternate setting number")
    class_code: int = Field(description="USB interface class code")
    subclass_code: int = Field(description="USB interface subclass code")
    protocol_code: int = Field(description="USB interface protocol code")
    endpoints: tuple[UsbEndpointDescriptor, ...] = Field(
        default=(), description="Endpoints in this alternate setting"
    )


class UsbCdcUnion(UsbModel):
    """CDC functional descriptor associating control and data interfaces."""

    master: int = Field(description="CDC control interface number")
    slaves: tuple[int, ...] = Field(default=(), description="Data interfaces")


class UsbInterfaceAssociation(UsbModel):
    """USB interface association descriptor."""

    first_interface: int = Field(description="First associated interface")
    interface_count: int = Field(description="Number of associated interfaces")
    class_code: int = Field(description="Function class code")
    subclass_code: int = Field(description="Function subclass code")
    protocol_code: int = Field(description="Function protocol code")


class MtpDeviceInfo(UsbModel):
    """Serial-redacted PTP device information."""

    standard_version: int = Field(description="PTP standard version")
    vendor_extension_id: int = Field(description="Vendor extension identifier")
    vendor_extension_version: int = Field(
        description="Vendor extension version"
    )
    vendor_extension_description: str = Field(
        description="Device-reported vendor extension name"
    )
    functional_mode: int = Field(description="PTP functional mode")
    supported_operation_codes: tuple[int, ...] = Field(
        default=(), description="Bounded supported operation codes"
    )
    manufacturer: str = Field(description="Device-reported manufacturer")
    model: str = Field(description="Device-reported model")
    device_version: str = Field(description="Device-reported version")


class MtpObjectInfo(UsbModel):
    """One bounded root object listing entry."""

    handle: int = Field(description="PTP object handle")
    format_code: int | None = Field(
        default=None, description="PTP object format", exclude_if=_absent
    )
    size_bytes: int | None = Field(
        default=None, description="Reported object size", exclude_if=_absent
    )
    name: str | None = Field(
        default=None,
        description="Device-reported object name",
        exclude_if=_absent,
    )
    error: str | None = Field(
        default=None, description="Request or decode error", exclude_if=_absent
    )
    response_code: int | None = Field(
        default=None, description="Rejected PTP response", exclude_if=_absent
    )


class MtpStorageInfo(UsbModel):
    """PTP storage metadata and optional bounded root listing."""

    id: int = Field(description="PTP storage identifier")
    storage_type: int | None = Field(
        default=None, description="Storage type", exclude_if=_absent
    )
    filesystem_type: int | None = Field(
        default=None, description="Filesystem type", exclude_if=_absent
    )
    access_capability: int | None = Field(
        default=None, description="Access capability", exclude_if=_absent
    )
    total_bytes: int | None = Field(
        default=None, description="Total capacity", exclude_if=_absent
    )
    free_bytes: int | None = Field(
        default=None, description="Available capacity", exclude_if=_absent
    )
    free_images: int | None = Field(
        default=None, description="Free image count", exclude_if=_absent
    )
    description: str | None = Field(
        default=None, description="Storage description", exclude_if=_absent
    )
    volume_label: str | None = Field(
        default=None, description="Volume label", exclude_if=_absent
    )
    root_object_count: int | None = Field(
        default=None, description="Root object count", exclude_if=_absent
    )
    root_objects: tuple[MtpObjectInfo, ...] = Field(
        default=(), description="Bounded root listing", exclude_if=_empty
    )
    root_listing_error: str | None = Field(
        default=None, description="Root listing error", exclude_if=_absent
    )
    root_listing_response_code: int | None = Field(
        default=None,
        description="Rejected root listing response",
        exclude_if=_absent,
    )
    error: str | None = Field(
        default=None, description="Storage request error", exclude_if=_absent
    )
    response_code: int | None = Field(
        default=None,
        description="Rejected storage response",
        exclude_if=_absent,
    )


class UsbProbe(UsbModel):
    """Known fields from one descriptor, MTP, or OBEX probe."""

    state: str = Field(default="device-unavailable", description="Probe state")
    backend: str = Field(
        default="libusb-static", description="Native USB backend"
    )
    detail: str | None = Field(
        default=None, description="Failure detail", exclude_if=_absent
    )
    response_code: int | None = Field(
        default=None, description="Protocol response code", exclude_if=_absent
    )
    scope: str = Field(
        default="", description="Operations performed", exclude_if=_empty
    )
    identity_basis: str | None = Field(
        default=None,
        description="Device selection evidence",
        exclude_if=_absent,
    )
    configuration: int | None = Field(
        default=None, description="Active USB configuration", exclude_if=_absent
    )
    interfaces: tuple[UsbInterfaceDescriptor, ...] = Field(
        default=(), description="Interface map", exclude_if=_empty
    )
    cdc_unions: tuple[UsbCdcUnion, ...] = Field(
        default=(), description="CDC unions", exclude_if=_empty
    )
    interface_associations: tuple[UsbInterfaceAssociation, ...] = Field(
        default=(), description="Interface associations", exclude_if=_empty
    )
    transport: str | None = Field(
        default=None, description="Protocol transport", exclude_if=_absent
    )
    interface: int | None = Field(
        default=None, description="Claimed interface number", exclude_if=_absent
    )
    device_info: MtpDeviceInfo | None = Field(
        default=None, description="PTP device information", exclude_if=_absent
    )
    storage: tuple[MtpStorageInfo, ...] = Field(
        default=(), description="PTP storage records", exclude_if=_empty
    )
    storage_count: int | None = Field(
        default=None, description="Reported storage count", exclude_if=_absent
    )
    session_closed: bool | None = Field(
        default=None, description="PTP session close result", exclude_if=_absent
    )
    target: str | None = Field(
        default=None, description="OBEX target", exclude_if=_absent
    )
    connection_id_present: bool | None = Field(
        default=None,
        description="OBEX connection ID observed",
        exclude_if=_absent,
    )
    disconnected: bool | None = Field(
        default=None, description="OBEX disconnect result", exclude_if=_absent
    )
    disconnect_error: str | None = Field(
        default=None, description="OBEX disconnect error", exclude_if=_absent
    )
    disconnect_response_code: int | None = Field(
        default=None, description="OBEX disconnect response", exclude_if=_absent
    )
    alternate_restored: bool | None = Field(
        default=None,
        description="Alternate setting restore result",
        exclude_if=_absent,
    )
    interface_released: bool | None = Field(
        default=None, description="Interface release result", exclude_if=_absent
    )
    response_bytes: int | None = Field(
        default=None,
        description="Invalid OBEX response size",
        exclude_if=_absent,
    )


class UsbDeviceDescriptor(UsbModel):
    """Public descriptor identity and host location without serial data."""

    vendor_id: int = Field(description="USB vendor identifier")
    product_id: int = Field(description="USB product identifier")
    bus: int = Field(description="Host USB bus number")
    address: int = Field(description="Host USB device address")
    ports: tuple[int, ...] = Field(default=(), description="Host port path")
    device_class: int = Field(description="USB device class")
    configuration_count: int = Field(description="Number of configurations")


class UsbPollFd(UsbModel):
    """File descriptor and poll event mask requested by libusb."""

    fd: int = Field(description="Host file descriptor")
    events: int = Field(description="Poll event mask")


class UsbCompletion(UsbModel):
    """Completed USB transfer returned by either async API."""

    id: int = Field(description="Transfer identifier")
    status: str = Field(description="Libusb completion state")
    actual_length: int = Field(description="Transferred byte count")
    data: bytes = Field(default=b"", description="Inbound payload")
