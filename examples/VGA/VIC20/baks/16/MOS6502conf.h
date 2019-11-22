#include "machine.h"


#define BUSREAD(addr)           ((Machine*)m_context)->busRead((addr))
#define BUSWRITE(addr, value)   ((Machine*)m_context)->busWrite((addr), (value))

#define PAGE0READ(addr)         ((Machine*)m_context)->page0Read((addr))
#define PAGE0WRITE(addr, value) ((Machine*)m_context)->page0Write((addr), (value))

#define PAGE1READ(addr)         ((Machine*)m_context)->page1Read((addr))
#define PAGE1WRITE(addr, value) ((Machine*)m_context)->page1Write((addr), (value))

