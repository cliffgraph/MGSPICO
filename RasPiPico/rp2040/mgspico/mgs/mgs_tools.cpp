#include <stdint.h>
#include <memory.h>
#include "mgs_tools.h"

static const char *IDSTR_MGSDRV = "MGSDRV";
static const int  SIZE_MGSDRV = 6;

bool t_Mgs_GetPtrBodyAndSize(
	const STR_MGSDRVCOM *p, const uint8_t **pBody, uint16_t *pSize)
{
	if( memcmp(p->id, IDSTR_MGSDRV, SIZE_MGSDRV) != 0 )
		return false;
	*pBody = reinterpret_cast<const uint8_t*>(&(p->startof_driver));
	*pSize = p->sizeof_driver;
	return true;
}

