/*****************************************************************
|
|    AP4 - enda Atom
|
 ****************************************************************/

/*----------------------------------------------------------------------
|       includes
+---------------------------------------------------------------------*/

#include "Ap4.h"
#include "Ap4EndaAtom.h"

/*----------------------------------------------------------------------
|       AP4_EndaAtom::AP4_EndaAtom
+---------------------------------------------------------------------*/

AP4_EndaAtom::AP4_EndaAtom(AP4_Size         size,
                           AP4_ByteStream&  stream)
	: AP4_Atom(AP4_ATOM_TYPE_ENDA)
{
	size -= AP4_ATOM_HEADER_SIZE;

    stream.ReadUI16(m_LittleEndian);
}
