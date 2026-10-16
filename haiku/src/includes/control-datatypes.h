#ifndef HAIKU_CONTROL_DATATYPES_H_INCLUDED
#define HAIKU_CONTROL_DATATYPES_H_INCLUDED 1

// A mapping between types used in service records and their associated underlying numerical type
// used in communication (control protocol)

#include <cstdint>

namespace haiku_cptypes {

using handle_t = uint32_t;
using trigger_val_t = uint8_t;
using cp_cmd_t = uint8_t;
using cp_rply_t = uint8_t;
using cp_info_t = uint8_t;
using srvname_len_t = uint16_t;
using envvar_len_t = uint16_t;
using sig_num_t = int;
using srvstate_t = uint8_t;

} // namespace haiku_cptypes

#endif // HAIKU_CONTROL_DATATYPES_H_INCLUDED
