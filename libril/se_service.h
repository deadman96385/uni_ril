#ifndef SE_SERVICE_H
#define SE_SERVICE_H

#include <telephony/ril.h>
#include <ril_internal.h>

namespace secureElement {

void registerService(SE_Functions *callbacks);

}   // namespace SecureElement

#endif  // SE_SERVICE_H
