#include <stdio.h>
#include <stdint.h>

struct LensGroup {
    
    // INT8_UNSIGNED* data;

    // void CalcApertureNormalized(int aperture) {
    //     data = { 0x00, };
    // }


    // Sets Focus
    // Minimum: 0.0 (near)
    // Maximum: 1.0 (far)
    // fixed16
    int Focus;
    // void Autofocus;
    
    // Aperture Value (where fnumber = sqrt(2^AV))
    // Minimum: -1
    // Maximum: 16
    // fixed16

    // int

    int ApertureFStop;

    // Sets a normalised aperture
    // Minimum: 0.0
    // Maximum: 1.0
    // fixed16
    int ApertureNormalised;

    // Steps trough available aperture values
    // Minimum: 0
    // Maximum: n
    uint16_t Aperture;

    // trigger instantanious auto aperture
    // void ApertureAuto;

    // Enable/Disable optical image stabilisation
    // Enable: true
    // Disable: false
    bool OpticalImageStabilisation;

    // Move to a specified focal lenth in mm
    // Minimum: 0
    // Maximum: max
    uint16_t ZoomAbsoluteMM;

    // Move to a specified focal length
    // Minimum: 0.0 (wide)
    // Maximum: 1.0 (tele)
    // fixed16
    int ZoomAbsoluteNormalised;

    // Start/Stop zooming at a specific rate
    // Minimum: -1
    // Maximum: 1
    // Stop: 0.0
    // fixed16
    int ZoomSpeed;
};
