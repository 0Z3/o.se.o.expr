/*
  Copyright (c) 2019-22 John MacCallum Permission is hereby granted,
  free of charge, to any person obtaining a copy of this software
  and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute,
  sublicense, and/or sell copies of the Software, and to permit
  persons to whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include <string.h>

#include "ose.h"
#include "ose_stackops.h"
#include "ose_assert.h"
#include "ose_vm.h"
#include "ose_print.h"
#include "ose_errno.h"

#include "osc.h"
#include "osc_expr.h"
#include "osc_expr_parser.h"
#include "osc_mem.h"
#include "osc_atom_u.h"
#include "osc_error.h"
#include "osc_bundle_iterator_s.h"
#include "osc_message_iterator_s.h"
#include "osc_typetag.h"

void ose_oexprParse(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(!ose_bundleHasAtLeastNElems(vm_s, 1))
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_COUNT);
        return;
    }
    if(ose_peekType(vm_s) != OSETT_MESSAGE)
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_TYPE);
        return;
    }
    if(ose_peekMessageArgType(vm_s) != OSETT_STRING)
    {
        ose_errno_set(vm_s, OSE_ERR_ITEM_TYPE);
        return;
    }
    const char * const str = ose_peekString(vm_s);
    t_osc_expr *e = NULL;
    t_osc_err err = osc_expr_parser_parseExpr((char *)str, &e, NULL);
    if(err != OSC_ERR_NONE)
    {
        ose_errno_set(vm_s, 128);
        return;
    }
    ose_drop(vm_s);
    ose_pushMessage(vm_s,
                    OSE_ADDRESS_ANONVAL,
                    OSE_ADDRESS_ANONVAL_LEN,
                    1, OSETT_ALIGNEDPTR, e);
}

void ose_oexprEval(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(!ose_bundleHasAtLeastNElems(vm_s, 2))
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_COUNT);
        return;
    }
    if(ose_peekType(vm_s) != OSETT_MESSAGE)
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_TYPE);
        return;
    }
    if(ose_peekMessageArgType(vm_s) != OSETT_BLOB)
    {
        ose_errno_set(vm_s, OSE_ERR_ITEM_TYPE);
        return;
    }
    ose_swap(vm_s);
    
    int32_t o = ose_getLastBundleElemOffset(vm_s);
    long s = ose_readInt32(vm_s, o);
    char *b = osc_mem_alloc(s);
    if(!b)
    {
        ose_errno_set(vm_s, 128);
        return;
    }
    memcpy(b, ose_getBundlePtr(vm_s) + o + 4, s);
    ose_drop(vm_s);
    
    t_osc_expr *e = (t_osc_expr *)ose_peekAlignedPtr(vm_s);
    if(!e)
    {
        ose_errno_set(vm_s, 128);
        return;
    }
    int ret = 0;
    ose_pushBundle(vm_s);
    while(e)
    {
        t_osc_atom_ar_u *av = NULL;
        ret = osc_expr_eval(e, &s, &b, &av, NULL);
        if(av)
        {
            ose_pushMessage(vm_s,
                            OSE_ADDRESS_ANONVAL,
                            OSE_ADDRESS_ANONVAL_LEN, 0);
            for(int i = 0; i < osc_atom_array_u_getLen(av); ++i)
            {
                t_osc_atom_u *a = osc_atom_array_u_get(av, i);
                char tt = osc_atom_u_getTypetag(a);
                if(OSC_TYPETAG_ISINT(tt)
                   || OSC_TYPETAG_ISBOOL(tt))
                {
                    ose_pushInt32(vm_s, osc_atom_u_getInt32(a));
                }
                else if(OSC_TYPETAG_ISFLOAT(tt))
                {
                    ose_pushFloat(vm_s, osc_atom_u_getFloat(a));
                }
                else if(OSC_TYPETAG_ISSTRING(tt))
                {
                    ose_pushString(vm_s, osc_atom_u_getStringPtr(a));
                }
                else if(tt == 'b')
                {
                    const int32_t bloblen = osc_atom_u_getBlobLen(a);
                    const char * const blob = osc_atom_u_getBlob(a);
                    ose_pushBlob(vm_s,
                                 bloblen,
                                 blob ? blob + 4 : NULL);
                }
                ose_push(vm_s);
            }
            ose_push(vm_s);
            osc_atom_array_u_free(av);
        }
        if(ret)
        {
            break;
        }
        e = osc_expr_next(e);
    }
    if(ret)
    {
        ose_errno_set(vm_s, 128);
    }
    // e is not freed
    ose_nip(vm_s);
    ose_pushBlob(vm_s, s, b);
    if(b)
    {
        osc_mem_free(b);
    }
}

void ose_oexprFree(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(!ose_bundleHasAtLeastNElems(vm_s, 1))
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_COUNT);
        return;
    }
    if(ose_peekType(vm_s) != OSETT_MESSAGE)
    {
        ose_errno_set(vm_s, OSE_ERR_ELEM_TYPE);
        return;
    }
    if(ose_peekMessageArgType(vm_s) != OSETT_BLOB)
    {
        ose_errno_set(vm_s, OSE_ERR_ITEM_TYPE);
        return;
    }
    t_osc_expr *e = (t_osc_expr *)ose_peekAlignedPtr(vm_s);
    if(!e)
    {
        ose_errno_set(vm_s, 128);
        return;
    }
    osc_expr_free(e);
    ose_drop(vm_s);
}

void ose_main(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    ose_pushBundle(vm_s);

    ose_pushMessage(vm_s, "/o.expr/parse", strlen("/o.expr/parse"),
                    1, OSETT_ALIGNEDPTR, ose_oexprParse);
    ose_push(vm_s);

    ose_pushMessage(vm_s, "/o.expr/eval", strlen("/o.expr/eval"),
                    1, OSETT_ALIGNEDPTR, ose_oexprEval);
    ose_push(vm_s);

    ose_pushMessage(vm_s, "/o.expr/free", strlen("/o.expr/free"),
                    1, OSETT_ALIGNEDPTR, ose_oexprFree);
    ose_push(vm_s);
}
