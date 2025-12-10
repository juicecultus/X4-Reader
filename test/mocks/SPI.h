#pragma once
#include "platform_stubs.h"

// Provide a simple alias so code including <SPI.h> works with the host mock
using SPIClass = MockSPI;
extern MockSPI SPI;
