/*
** LCLint - annotation-assisted static program checker
** Copyright (C) 1994-2000 University of Virginia,
**         Massachusetts Institute of Technology
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version.
** 
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** The GNU General Public License is available from http://www.gnu.org/ or
** the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
** MA 02111-1307, USA.
**
** For information on lclint: lclint-request@cs.virginia.edu
** To report a bug: lclint-bug@cs.virginia.edu
** For more information: http://lclint.cs.virginia.edu
*/
/*
** ctype.c
**
** This files implements three types: ctentry, cttable and ctype.
** They should probably be separated soon.
*/

# include "lclintMacros.nf"
# include "basic.h"
# include "structNames.h"

static void ctype_recordConj (ctype p_c);


/*
** ctbase file
*/

# include "ctbase.i"

/*
** ctype table
*/

# include "cttable.i"

static ctype ctype_getConjA (ctype p_c) /*@*/ ;
static ctype ctype_getConjB (ctype p_c) /*@*/ ;

static bool ctype_isComplex (ctype c)
{
  return (ctentry_isComplex (ctype_getCtentry(c)));
}

static bool ctype_isPlain (ctype c)
{
  return (ctentry_isPlain (ctype_getCtentry(c)));
}

static bool ctype_isBroken (ctype c)
{
  /*@+enumint@*/
  if (c == CTK_DNE || c == CTK_INVALID || c == CTK_UNKNOWN)
    {
      /*@-enumint@*/
      return TRUE;
    }
  else
    {
      ctentry cte = ctype_getCtentry (c);

      return (ctentry_isBogus (cte));
    }
}

ctkind
ctkind_fromInt (int i)
{
  /*@+enumint@*/
  if (i < CTK_UNKNOWN || i > CTK_COMPLEX)
    {
      llcontbug (message ("ctkind_fromInt: out of range: %d", i));
      return CTK_INVALID;
    }
  return (ctkind) i;
  /*@=enumint@*/
}

/*
** ctype functions
*/

void
ctype_initTable ()
{
  cttable_init ();
}

void
ctype_destroyMod ()
{
  cttable_reset ();
}

void
ctype_loadTable (FILE *f)
{
  cttable_load (f);
}

void
ctype_dumpTable (FILE *f)
{
  DPRINTF (("Dumping cttable!"));
  cttable_dump (f);
}

cstring
ctype_unparseTable ()
{
  return (cttable_unparse ());
}

void
ctype_printTable ()
{
  cttable_print ();
}

bool
ctype_isUserBool (ctype ct)
{
  if (ctype_isUA (ct))
    {
      return (usymtab_isBoolType (ctype_typeId (ct)));
    }
  
  return (FALSE);
}

ctype
ctype_createUser (typeId u)
{
  /* requires: ctype_createUser (u) is never called more than once for any u. */

  ctbase ct = ctbase_createUser (u);
  return (cttable_addFullSafe (ctentry_makeNew (CTK_PLAIN, ct)));
}

ctype
ctype_createAbstract (typeId u)
{
 /* requires: ctype_createAbstract (u) is never called more than once for any u. */
 /*           [ tested by cttable_addFullSafe, not really required ]            */
  return (cttable_addFullSafe (ctentry_makeNew (CTK_PLAIN, ctbase_createAbstract (u))));
}

int
ctype_count (void)
{
  return (cttab.size);
}

ctype
ctype_realType (ctype c)
{
  ctype r = c;

  if (ctype_isUA (c))
    {
      r = uentry_getRealType (usymtab_getTypeEntry (ctype_typeId (c)));
    }
  
  if (ctype_isManifestBool (r))
    {
      if (context_canAccessBool ())      
	{
	  r = context_boolImplementationType ();
	}
    }
  
  return r;
}

bool
ctype_isSimple (ctype c)
{
  return (!(ctype_isPointer (c) 
	    || ctype_isArray (c)
	    || ctype_isFunction (c)));
}

ctype
ctype_forceRealType (ctype c)
{
  ctype r = c;

  if (ctype_isUA (c))
    {
      r = uentry_getForceRealType (usymtab_getTypeEntry (ctype_typeId (c)));
    }
  
  return r;
}

ctype
ctype_realishType (ctype c)
{
  if (ctype_isUA (c))
    {
      if (ctype_isManifestBool (c))
	{
	  return ctype_bool;
	}
      else
	{
	  ctype r = uentry_getRealType (usymtab_getTypeEntry 
					(ctype_typeId (c)));
	  return (r);
	}
    }

  return c;
}

bool
ctype_isUA (ctype c)
{
  return (ctbase_isUA (ctype_getCtbase (c)));
}

bool
ctype_isUser (ctype c)
{
  return (ctbase_isUser (ctype_getCtbase (c)));
}

bool
ctype_isAbstract (ctype c)
{
  return ((ctype_isPlain (c) && ctbase_isAbstract (ctype_getCtbaseSafe (c))) ||
	  (ctype_isConj (c) &&
	   (ctype_isAbstract (ctype_getConjA (c)) 
	    || ctype_isAbstract (ctype_getConjB (c)))));
}

bool
ctype_isRealAbstract (ctype c)
{
  return (ctype_isAbstract (ctype_realType (c)) ||
	  (ctype_isConj (c) && 
	   (ctype_isRealAbstract (ctype_getConjA (c)) || 
	    ctype_isRealAbstract (ctype_getConjB (c)))));
}

/*
** primitive creators
*/

/*
** createPrim not necessary --- subsumed by ctype_int, etc.
*/

/*
** ctbase_unknown --- removed argument
*/

/*
** derived types:
**    requires:  if DerivedType (T) exists in cttable, then T->derivedType is it.
*/

ctype
ctype_makePointer (ctype c)
{
  if (c == ctype_char)
    {
      return ctype_string;
    }
  else if (c == ctype_void)
    {
      return ctype_voidPointer;
    }
  else
    {
      ctentry cte = ctype_getCtentry (c);
      ctype clp = ctentry_getPtr (cte);
      
      if /*@+enumint@*/ (clp == CTK_DNE) /*@=enumint@*/
	{
	  ctype cnew = cttable_addDerived (CTK_PTR, ctbase_makePointer (c), c);
	  ctentry_setPtr (cte, cnew);
	  	  return (cnew);
	}
      else
	{
	  	  return clp;
	}
    }
}

ctype ctype_makeFixedArray (ctype c, long size)
{
  return (cttable_addDerived (CTK_ARRAY, ctbase_makeFixedArray (c, size), c));
}

ctype
ctype_makeArray (ctype c)
{
  ctentry cte = ctype_getCtentry (c);
  ctype clp = ctentry_getArray (cte);

  if /*@+enumint@*/ (clp == CTK_DNE) /*@=enumint@*/
    {
      ctype cnew = cttable_addDerived (CTK_ARRAY, ctbase_makeArray (c), c);
      ctentry_setArray (cte, cnew);
      return (cnew);
    }
  else
    return clp;
}

/*
** requires c is a pointer of array
*/

ctype
ctype_baseArrayPtr (ctype c)
{
  ctentry cte = ctype_getCtentry (ctype_realType (c));

  if (ctype_isConj (c))
    {
      if (ctype_isAP (ctype_getConjA (c)))
	{
	  if (ctype_isAP (ctype_getConjB (c)))
	    {
	      return (ctype_makeConj (ctype_baseArrayPtr (ctype_getConjA (c)),
				      ctype_baseArrayPtr (ctype_getConjB (c))));
	    }
	  else
	    {
	      return (ctype_baseArrayPtr (ctype_getConjA (c)));
	    }
	}
      else
	{
	  return (ctype_baseArrayPtr (ctype_getConjB (c)));
	}
    }
  else if (ctype_isInt (c)) /* could be NULL */
    {
      return ctype_unknown;
    }
  else
    {
      ctype clp = ctentry_getBase (cte);

      if (ctype_isBroken (clp))
	{
	  llbuglit ("ctype_baseArrayPtr: bogus ctype");
	}

      return clp;
    }
}

ctype
ctype_returnValue (ctype c)
{
  return (ctbase_baseFunction (ctype_getCtbaseSafe (c)));
}

/*
** must be a shared pointer
*/

/*@observer@*/ uentryList
ctype_argsFunction (ctype c)
{
  return (ctbase_argsFunction (ctype_getCtbaseSafe (c)));
}

/*
** Returns type with base type p and compound types from c.
**
** i.e.,  c = char *[]; p = int
**     => int *[]
*/

ctype
ctype_newBase (ctype c, ctype p)
{
  return (ctbase_newBase (c, p));
}

bool
ctype_sameAltTypes (ctype c1, ctype c2)
{
  ctype c1a, c2a;
  ctype c1b, c2b;

  llassert (ctype_isConj (c1) && ctype_isConj (c2));

  c1a = ctype_getConjA (c1);
  c2a = ctype_getConjA (c2);

  c1b = ctype_getConjB (c1);
  c2b = ctype_getConjB (c2);

  if (ctype_compare (c1a, c2a) == 0)
    {
      if (ctype_compare (c1b, c2b) == 0)
	{
	  return TRUE;
	}
      else
	{
	  if (ctype_isConj (c1b) && ctype_isConj (c2b))
	    {
	      return ctype_sameAltTypes (c1b, c2b);
	    }
	  else
	    {
	      return FALSE;
	    }
	}
    }
  else
    {
      if (ctype_compare (c1a, c2b) == 0)
	{
	  if (ctype_compare (c1b, c2a) == 0)
	    {
	      return TRUE;
	    }
	  else
	    {
	      if (ctype_isConj (c1b) && ctype_isConj (c2a))
		{
		  return ctype_sameAltTypes (c1b, c2a);
		}
	      else
		{
		  return FALSE;
		}
	    }
	}
      else
	{
	  return FALSE;
	}
    }
}

int
ctype_compare (ctype c1, ctype c2)
{
  ctentry ce1;
  ctentry ce2;

  /* Can't get entries for special ctypes (elips marker) */

  if (ctype_isElips (c1) || ctype_isElips (c2)
      || ctype_isMissingParamsMarker (c1) || ctype_isMissingParamsMarker (c2)) {
    return int_compare (c1, c2);
  }

  ce1 = ctype_getCtentry (c1);
  ce2 = ctype_getCtentry (c2);

  if (ctentry_isComplex (ce1))
    {
      if (ctentry_isComplex (ce2))
	{
	  return (ctbase_compare (ctype_getCtbase (c1),
				  ctype_getCtbase (c2), FALSE));
	}
      else
	{
	  return 1;
	}
    }
  else if (ctentry_isComplex (ce2))
    {
      return -1;
    }
  else
    {
      return (int_compare (c1, c2));
    }
}

/*
** complex types
*/

/*
** makeFunction:  pointer to function returning base
*/

ctype
ctype_makeParamsFunction (ctype base, /*@only@*/ uentryList p)
{
  uentryList_fixImpParams (p);
  return (ctype_makeFunction (base, p));
}

ctype
ctype_makeNFParamsFunction (ctype base, /*@only@*/ uentryList p)
{
  uentryList_fixImpParams (p);
  return (ctbase_makeNFFunction (base, p));
}

ctype
ctype_makeFunction (ctype base, /*@only@*/ uentryList p)
{
  ctype ret;
  ret = ctbase_makeFunction (base, p);
  return (ret);
}

ctype ctype_expectFunction (ctype c)
{
  /* handle parenthesized declarations */

  if (!ctype_isAP (c))
    {
      c = ctype_makePointer (c);
    }

  return (cttable_addComplex (ctbase_expectFunction (c)));
}

/*
** makeRealFunction: function returning base
*/

ctype ctype_makeRealFunction (ctype base, uentryList p)
{
  return (cttable_addComplex (ctbase_makeRealFunction (base, p)));
}

/*
** plain predicates
*/

/***
**** this is very poorly defined
****
**** need to unify function/function pointer meaning
***/

bool
ctype_isFunction (ctype c)
{
  return (ctbase_isFunction (ctype_getCtbase (c)));
}

bool
ctype_isExpFcn (ctype c)
{
  return (ctbase_isExpFcn (ctype_getCtbase (c))); 
}

bool
ctype_isVoid (ctype c)
{
  return (c == CTX_VOID);
}

bool
ctype_isArbitraryIntegral (ctype c)
{
  ctype cr = ctype_realType (c);

  return (cr == ctype_anyintegral || cr == ctype_unsignedintegral 
	  || cr == ctype_signedintegral);
}

bool
ctype_isUnsignedIntegral (ctype c)
{
  ctype cr = ctype_realType (c);

  return (cr == ctype_unsignedintegral);
}

bool
ctype_isSignedIntegral (ctype c)
{
  ctype cr = ctype_realType (c);

  return (cr == ctype_signedintegral);
}

bool
ctype_isInt (ctype c)
{
  cprim cp = ctype_toCprim (c);

  return (c == ctype_unknown || cprim_isAnyInt (cp)
	  || (cprim_isAnyChar (cp) && context_msgCharInt ())
	  || (c == ctype_bool && context_msgBoolInt ()) 
	  || (ctype_isEnum (c) && context_msgEnumInt ()));
}

bool
ctype_isRegularInt (ctype c)
{
  cprim cp = ctype_toCprim (c);

  return (c == ctype_unknown
	  || cprim_closeEnough (cprim_int, cp)
	  || (cprim_isAnyChar (cp) && context_msgCharInt ())
	  || (c == ctype_bool && context_msgBoolInt ()) 
	  || (ctype_isEnum (c) && context_msgEnumInt ()));
}

bool
ctype_isString (ctype c)
{
  return (c == ctype_string 
	  || (ctype_isPointer (c)
	      && ctype_isChar (ctype_baseArrayPtr (c))));
}

bool
ctype_isChar (ctype c)
{
   return ((c == ctype_unknown) || (cprim_isAnyChar (ctype_toCprim (c)))
	  || (context_getFlag (FLG_CHARINT) && ctype_isInt (c))); 
}

bool
ctype_isUnsignedChar (ctype c)
{
  return ((c == ctype_unknown) || (cprim_isUnsignedChar (ctype_toCprim (c))));
}

bool
ctype_isSignedChar (ctype c)
{
  return ((c == ctype_unknown) || (cprim_isSignedChar (ctype_toCprim (c))));
}

/*
** Returns true if c matches the name -booltype <bool>
*/

bool
ctype_isManifestBool (ctype c)
{
  /*
  ** Changed the meaning of ctype_isBool - evs 2000-07-24
  ** The old meaning was very convoluted!
  **
  ** c is a bool if:
  **       c == CTX_BOOL - its a direct bool
  **       c is a user/abstract type matching the bool name
  **            (should never occur?)
  */

  if (ctype_isDirectBool (c)) {
    return TRUE;
  } else if (ctype_isUA (c)) {
    return ctype_isUserBool (c);
  } else {
    return FALSE;
  }
}

bool
ctype_isBool (ctype c)
{
  /*
  ** Changed the meaning of ctype_isBool - evs 2000-07-24
  ** The old meaning was very convoluted!
  **
  ** c is a bool if:
  **       its a manifest bool
  **       +boolint and ctype_isInt (c)
  */

  if (ctype_isManifestBool (c)) {
    return TRUE;
  } else if (context_msgBoolInt ()) {
    return ctype_isInt (c);
  } else {
    return FALSE;
  }

# if 0
  if (context_getFlag (FLG_ABSTRACTBOOL))
    {
      if (typeId_isInvalid (boolType))
	{ 
	  boolType = usymtab_getTypeId (context_getBoolName ());
	} 
      
      if (context_hasAccess (boolType))
	{
	  return (((c == CTX_UNKNOWN) || (c == CTX_BOOL) 
		   || (context_msgBoolInt () 
		       && (c == CTX_INT 
			   || (c == CTX_CHAR && context_msgCharInt ()))))
		  || ctype_isInt (c));
	}
    }
  
  return ((c == CTX_UNKNOWN) || (c == CTX_BOOL)
	  || (context_msgBoolInt ()
	      && (c == CTX_INT || (c == CTX_CHAR && context_msgCharInt ()))));
# endif
}

bool
ctype_isDirectBool (ctype c)
{
  return (c == CTX_BOOL);
}

bool
ctype_isReal (ctype c)
{
  return (cprim_isAnyReal (ctype_toCprim (c)));
}

bool
ctype_isFloat (ctype c)
{
  return (c == ctype_float);
}

bool
ctype_isDouble (ctype c)
{
  return (c == ctype_double || c == ctype_ldouble);
}

bool
ctype_isSigned (ctype c)
{
  return (!ctype_isUnsigned (c));
}

bool
ctype_isNumeric (ctype c)
{
  return (ctype_isInt (c) || ctype_isReal (c) || ctype_isEnum (c));
}


/*
** real predicates
**
** work on actual type in current context
*/

bool
ctype_isRealNumeric (ctype c)
{
  if (ctype_isPlain (c))
    return (ctype_isNumeric (ctype_realType (c)));
  if (ctype_isConj (c))
    return (ctype_isRealNumeric (ctype_getConjA (c)) ||
	    ctype_isRealNumeric (ctype_getConjB (c)));
  else
    return FALSE;
}

bool
ctype_isRealInt (ctype c)
{
  if (ctype_isPlain (c))
    return (ctype_isInt (ctype_realType (c)));
  else if (ctype_isConj (c))
    return (ctype_isRealInt (ctype_getConjA (c)) ||
	    ctype_isRealInt (ctype_getConjB (c)));
  else
    {
      if (ctype_isEnum (c) && context_msgEnumInt ()) return TRUE;
      return FALSE;
    }
}

bool
ctype_isRealVoid (ctype c)
{
  if (ctype_isPlain (c))
    {
      return (ctype_isVoid (ctype_realType (c)));
    }
  else if (ctype_isConj (c))
    {
      return (ctype_isRealVoid (ctype_getConjA (c)) ||
	      ctype_isRealVoid (ctype_getConjB (c)));
    }
  else
    {
      return FALSE;
    }
}

bool
ctype_isRealBool (ctype c)
{
  if (ctype_isPlain (c))
    {
      return (ctype_isBool (ctype_realishType (c)));
    }
  else if (ctype_isConj (c))
    {
      return (ctype_isRealBool (ctype_getConjA (c)) ||
	      ctype_isRealBool (ctype_getConjB (c)));
    }
  else
    {
      return FALSE;
    }
}

bool
ctype_isRealPointer (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isRealPointer (ctype_getConjA (c)) ||
	    ctype_isRealPointer (ctype_getConjB (c)));
  return (ctype_isPointer (ctype_realType (c)));
}

bool
ctype_isRealSU (ctype c)
{
  if (ctype_isConj (c))
    {
      return (ctype_isRealSU (ctype_getConjA (c)) ||
	      ctype_isRealSU (ctype_getConjB (c)));
    }
  return (ctype_isStructorUnion (ctype_realType (c)));
}
  
bool
ctype_isRealArray (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isRealArray (ctype_getConjA (c)) ||
	    ctype_isRealArray (ctype_getConjB (c)));
  return (ctype_isArray (ctype_realType (c)));
}

bool
ctype_isRealAP (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isRealAP (ctype_getConjA (c)) ||
	    ctype_isRealAP (ctype_getConjB (c)));
  return (ctype_isAP (ctype_realType (c)));
}

bool
ctype_isRealFunction (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isRealFunction (ctype_getConjA (c)) ||
	    ctype_isRealFunction (ctype_getConjB (c)));
  return (ctype_isFunction (ctype_realType (c)));
}

bool
ctype_isDirectInt (ctype c)
{
  return (c == CTX_INT || c == CTX_UINT || c == CTX_SINT || c == CTX_ULINT || c == CTX_USINT);
}

/*
** forceful predicates
**
** take *ctype; if its a conjunct, and there is a match replace with match only.
**                                 if both match, still conjunct
*/

static bool
  ctype_isForcePred (ctype * c, bool (pred) (ctype))
{
  if (ctype_isConj (*c))
    {
      ctype cbr = ctype_getConjA (*c);

      if ((*pred) (cbr))
	{
	  if ((*pred) (ctype_getConjB (*c)))
	    {
	      ;
	    }
	  else
	    {
	      *c = cbr;
	    }

	  return TRUE;
	}
      else 
	{
	  if ((*pred) (cbr = ctype_getConjB (*c)))
	    {
	      *c = cbr;
	      return TRUE;
	    }
	}
    }

  return ((*pred) (*c));
}

bool
ctype_isForceRealNumeric (ctype * c)
{
  return (ctype_isForcePred (c, ctype_isRealNumeric));
}

bool
ctype_isForceRealInt (ctype * c)
{
  return (ctype_isForcePred (c, ctype_isRealInt));
}

bool
ctype_isForceRealBool (ctype * c)
{
  return (ctype_isForcePred (c, ctype_isRealBool));
}

/*
** conjuncts
**
** save int/char, int/bool, other random conjuncts
*/

static ctype
ctype_makeConjAux (ctype c1, ctype c2, bool isExplicit)
{
  if (ctype_isBogus (c1) || ctype_isUndefined (c1))
    {
      return c2;
    }
  else if (ctype_isBogus (c2) || ctype_isUndefined (c2))
    {
      return c1;
    }
  else
    {
      if (isExplicit)
	{
	  return (ctype_makeExplicitConj (c1, c2));
	}
      else
	{
	  return (ctype_makeConj (c1, c2));
	}
    }
}

ctype
ctype_makeExplicitConj (ctype c1, ctype c2)
{
  if (ctype_isFunction (c1) && !ctype_isFunction (c2))
    {
      ctype ret = ctype_makeExplicitConj (ctype_returnValue (c1), c2);

      return ctype_makeFunction (ret, uentryList_copy (ctype_getParams (c1)));
    }
  else if (ctype_isFunction (c2) && !ctype_isFunction (c1))
    {
      ctype ret = ctype_makeExplicitConj (c1, ctype_returnValue (c2));

      return ctype_makeFunction (ret, uentryList_copy (ctype_getParams (c2)));
    }
  else
    {
      return (cttable_addComplex (ctbase_makeConj (c1, c2, TRUE)));
    }
}

static ctype ic = ctype_unknown;   /* int | char */
static ctype ib = ctype_unknown;   /* int | bool */
static ctype ifl = ctype_unknown;  /* int | float */
static ctype ibf = ctype_unknown;  /* int | bool | float */
static ctype ibc = ctype_unknown;  /* int | bool | char */
static ctype iv = ctype_unknown;   /* int | void * */
static ctype ivf = ctype_unknown;  /* int | void * | float */
static ctype ivb = ctype_unknown;  /* int | void * | bool */
static ctype ivbf = ctype_unknown; /* int | void * | bool | float */
static ctype cuc = ctype_unknown;  /* char | unsigned char */

static void
ctype_recordConj (ctype c)
{
  ctype c1, c2;
  
  llassert (ctype_isConj (c));

  c1 = ctype_getConjA (c);
  c2 = ctype_getConjB (c);

  /* No, can't swap!
  if (c2 == ctype_int && c1 != ctype_int) 
    {
      ctype tmp;

      tmp = c1;
      c1 = c2;
      c2 = tmp;
    }
    */

  if (c1 == ctype_int)
    {
      if (c2 == ctype_char)
	{
	  llassert (ic == ctype_unknown);
	  ic = c;
	}
      else if (c2 == ctype_bool)
	{
	  llassert (ib == ctype_unknown);
	  ib = c;
	}
      else if (c2 == ctype_float)
	{
	  llassert (ifl == ctype_unknown);
	  ifl = c;
	}
      else if (c2 == CTP_VOID)
	{
	  llassert (iv == ctype_unknown); 
	  iv = c;
	}
      else
	{
	  /* not special */
	}
    }
  else if (c1 == ib && ib != ctype_unknown)
    {
      if (c2 == ctype_float)
	{
	  llassert (ibf == ctype_unknown);
	  ibf = c;
	}	 
      else if (c2 == ctype_char)
	{
	  llassert (ibc == ctype_unknown);
	  ibc = c;
	}	
      else
	{
	  /* not special */
	}
    }
  else if (c1 == iv)
    {
      if (c2 == ctype_bool)
	{
	  llassert (ivb == ctype_unknown);
	  ivb = c;
	}
      else if (c2 == ctype_float)
	{
	  llassert (ivf == ctype_unknown);
	  ivf = c;
	}
      else
	{
	  /* not special */
	}
    }
  else if (c1 == ivf)
    {
      if (c2 == ctype_bool)
	{
	  llassert (ivbf == ctype_unknown);
	  ivbf = c;
	}
    }
  else if (c1 == ivb)
    {
      if (c2 == ctype_float)
	{
	  llassert (ivbf == ctype_unknown);
	  ivbf  = c;
	}
    }
  else if (c1 == ctype_char)
    {
      if (c2 == ctype_uchar)
	{
	  llassert (cuc == ctype_unknown);

	  cuc = c;
	}
    }
  else
    {
      /* not special */
    }
}

ctype
ctype_makeConj (ctype c1, ctype c2)
{
  /* no: can have unsigned long @alt long@: llassert (c1 != c2); */

  DPRINTF (("Make conj: %s / %s", ctype_unparse (c1), ctype_unparse (c2)));

  if (ctype_isFunction (c1) && !ctype_isFunction (c2))
    {
      ctype ret = ctype_makeConj (ctype_returnValue (c1), c2);
      return ctype_makeFunction (ret, uentryList_copy (ctype_getParams (c1)));
    }
  else if (ctype_isFunction (c2) && !ctype_isFunction (c1))
    {
      ctype ret = ctype_makeConj (c1, ctype_returnValue (c2));
      return ctype_makeFunction (ret, uentryList_copy (ctype_getParams (c2)));
    }
  else
    {
      if (ctype_isManifestBool (c1))
	{
	  c1 = ctype_bool;
	}
      
      if (ctype_isManifestBool (c2))
	{
	  c2 = ctype_bool;
	}
      
      if (ctbase_isVoidPointer (ctype_getCtbaseSafe (c1)))
	{
	  c1 = ctype_voidPointer;
	}
      
      if (ctbase_isVoidPointer (ctype_getCtbaseSafe (c2)))
	{
	  c2 = ctype_voidPointer;
	}

      /*
      ** Ouch, can't do this.  unsigned, etc. modifiers might
      ** apply to wrong type!
      **
      ** if (c2 == ctype_int && c1 != ctype_int) 
      ** {
      **  ctype tmp;
      **
      **  tmp = c1;
      **  c1 = c2;
      **  c2 = tmp;
      ** }
      **
      */

      if (c1 == ctype_int)
	{
	  if (c2 == ctype_char)
	    {
	      if (ic == ctype_unknown)
		{
		  ic = cttable_addComplex (ctbase_makeConj (ctype_int, ctype_char, FALSE));
		}
	      
	      return ic;
	    }
	  else if (c2 == ctype_bool)
	    {
	      if (ib == ctype_unknown)
		{
		  ib = cttable_addComplex 
		    (ctbase_makeConj (ctype_int, ctype_bool, FALSE));
		}
	      
	      return ib;
	    }
	  else if (c2 == ctype_float)
	    {
	      if (ifl == ctype_unknown)
		{
		  ifl = cttable_addComplex (ctbase_makeConj (ctype_int, ctype_float, FALSE));
		}
	      
	      return ifl;
	    }
	  else 
	    {
	      if (c2 == ctype_voidPointer)
		{
		  if (iv == ctype_unknown)
		    {
		      iv = cttable_addComplex
			(ctbase_makeConj (ctype_int, 
					  ctype_voidPointer,
					  FALSE));
		    }
		  
		  return iv;
		}
	    }
	}
      else if (c1 == ib && ib != ctype_unknown)
	{
	  if (c2 == ctype_float)
	    {
	      if (ibf == ctype_unknown)
		{
		  ibf = cttable_addComplex (ctbase_makeConj (ib, ctype_float, FALSE));
		}
	      
	      return ibf;
	    }	 
	  else if (c2 == ctype_char)
	    {
	      if (ibc == ctype_unknown)
		{
		  ibc = cttable_addComplex (ctbase_makeConj (ib, ctype_char, FALSE));
		}
	      
	      return ibc;
	    }	 
	  else
	    {
	      ;
	    }
	}
      else if (c1 == iv)
	{
	  if (c2 == ctype_bool)
	    {
	      if (ivb == ctype_unknown)
		{
		  ivb = cttable_addComplex (ctbase_makeConj (c1, c2, FALSE));
		}
	      
	      return ivb;
	    }
	  else if (c2 == ctype_float)
	    {
	      if (ivf == ctype_unknown)
		{
		  ivf = cttable_addComplex (ctbase_makeConj (c1, c2, FALSE));
		}
	      
	      return ivf;
	    }
	  else
	    {
	      ;
	    }
	}
      else if (c1 == ivf)
	{
	  if (c2 == ctype_bool)
	    {
	      if (ivbf == ctype_unknown)
		{
		  ivbf = cttable_addComplex (ctbase_makeConj (c1, c2, FALSE));
		}
	      
	      return ivbf;
	    }
	}
      else if (c1 == ivb)
	{
	  if (c2 == ctype_float)
	    {
	      if (ivbf == ctype_unknown)
		{
		  ivbf = cttable_addComplex (ctbase_makeConj (c1, c2, FALSE));
		}
	      
	      	      return ivbf;
	    }
	}
      else if (c1 == ctype_char)
	{
	  if (c2 == ctype_uchar)
	    {
	      if (cuc == ctype_unknown)
		{
		  cuc = cttable_addComplex (ctbase_makeConj (c1, c2, FALSE));
		}
	      
	      	      return cuc;
	    }
	}
      else
	{
	  ;
	}

      
      return (cttable_addComplex (ctbase_makeConj (c1, c2, FALSE)));
    }
}


bool
ctype_isConj (ctype c)
{
  return (ctype_isComplex (c) && ctbase_isConj (ctype_getCtbase (c)));
}

static ctype
ctype_getConjA (ctype c)
{
  if (!ctype_isConj (c))
    llbuglit ("ctype_getConjA: not a conj");
  return (ctbase_getConjA (ctype_getCtbaseSafe (c)));
}

static ctype
ctype_getConjB (ctype c)
{
  if (!ctype_isConj (c))
    llbuglit ("ctype_getConjB: not a conj");
  return (ctbase_getConjB (ctype_getCtbaseSafe (c)));
}

static bool
ctype_isExplicitConj (ctype c)
{
  return (ctype_isConj (c) && ctbase_isExplicitConj (ctype_getCtbaseSafe (c)));
}

/** << need to fix resolveConj >> **/

/*
** structs and unions
*/

ctype
ctype_createStruct (/*@only@*/ cstring n, /*@only@*/ uentryList f)
{
  ctype ct;

  DPRINTF (("Creating a struct: %s / %s",
	    n, uentryList_unparse (f)));

  ct = cttable_addComplex (ctbase_createStruct (n, f));
  DPRINTF (("ct: %s", ctype_unparse (ct)));
  return (ct);
}

uentryList
ctype_getFields (ctype c)
{
  return (ctbase_getuentryList (ctype_getCtbaseSafe (c)));
}

ctype
ctype_createUnion (/*@only@*/ cstring n, /*@only@*/ uentryList f)
{
  ctype ret;

  ret = cttable_addComplex (ctbase_createUnion (n, f));
  return ret;
}

/*
** matching
**
** if ctype's are same, definite match.
** else, need to call ctbase_match.
**
** if necessary context can memoize matches
*/

static bool
  quickMatch (ctype c1, ctype c2)
{
  if (c1 == c2)
    return TRUE;

  return FALSE;
}

bool
ctype_genMatch (ctype c1, ctype c2, bool force, bool arg, bool def, bool deep)
{
  bool match;

  DPRINTF (("Gen match: %s / %s arg: %s", ctype_unparse (c1), ctype_unparse (c2), bool_unparse (arg)));

  if (quickMatch (c1, c2))
    {
      return TRUE;
    }

  if (ctype_isElips (c1) || ctype_isElips (c2))
    {
      return FALSE;
    }
  else
    {
      match = ctbase_genMatch (ctype_getCtbase (c1), ctype_getCtbase (c2), force, arg, def, deep);
      return (match);
    }
}

bool
ctype_sameName (ctype c1, ctype c2)
{
  if (quickMatch (c1, c2))
    return TRUE;
  else
    return (cstring_equal (ctype_unparse (c1), ctype_unparse (c2)));
}

bool 
ctype_almostEqual (ctype c1, ctype c2)
{
  if (ctype_equal (c1, c2))
    {
      return TRUE;
    }
  else
    {
      return (ctbase_almostEqual (ctype_getCtbase (c1), ctype_getCtbase (c2)));
    }
}
 
bool
ctype_matchDef (ctype c1, ctype c2)
{
  DPRINTF (("Match def: %s / %s", ctype_unparse (c1), ctype_unparse (c2)));

  if (quickMatch (c1, c2))
    return TRUE;

  if (ctype_isElips (c1))
    return (ctype_isElips (c2) || ctype_isUnknown (c2));

  if (ctype_isElips (c2))
    return (ctype_isUnknown (c2));
  else
    {
      bool oldrelax = context_getFlag (FLG_RELAXQUALS);
      bool res;

      context_setFlagTemp (FLG_RELAXQUALS, FALSE);
      res = ctbase_matchDef (ctype_getCtbase (c1), ctype_getCtbase (c2));
      context_setFlagTemp (FLG_RELAXQUALS, oldrelax);
      return res;
    }
}

bool ctype_match (ctype c1, ctype c2)
{
  if (quickMatch (c1, c2))
    return TRUE;

  if (ctype_isElips (c1))
    return (ctype_isElips (c2) || ctype_isUnknown (c2));

  if (ctype_isElips (c2))
    return (ctype_isUnknown (c2));
 
  return (ctbase_match (ctype_getCtbase (c1), ctype_getCtbase (c2)));
}

bool
ctype_forceMatch (ctype c1, ctype c2)
{
  if (quickMatch (c1, c2))
    return TRUE;

  if (ctype_isElips (c1))
    return (ctype_isElips (c2));

  if (ctype_isElips (c2))
    return FALSE;

  /*@-modobserver@*/
  /* The call forceMatch may modify the observer params, but, we don't care. */
  return (ctbase_forceMatch (ctype_getCtbase (c1), ctype_getCtbase (c2)));
  /*@=modobserver@*/
}

bool
ctype_matchArg (ctype c1, ctype c2)
{
  if (quickMatch (c1, c2))
    {
      return TRUE;
    }
  else
    {
      return (ctbase_matchArg (ctype_getCtbase (c1), ctype_getCtbase (c2)));
    }
}

/*
** simple ctype_is operations.
** DO NOT use real type of c, only direct type.
*/

/*
** ctype_isVoidPointer
**
** void *
*/

bool
ctype_isVoidPointer (ctype c)
{
  if (ctype_isComplex (c))
    {
      return ctbase_isVoidPointer (ctype_getCtbaseSafe (c));
    }
  if (ctype_isConj (c))
    {
      return (ctype_isVoidPointer (ctype_getConjA (c)) ||
	      ctype_isVoidPointer (ctype_getConjB (c)));
    }
  else
    {
      return (c == ctype_voidPointer
	      || (ctype_isRealPointer (c) 
		  && ctype_isVoid (ctype_baseArrayPtr (c))));
    }
}

/*
** ctype_isPointer
**
** true for C and LCL pointers
*/

bool
ctype_isPointer (ctype c)
{
  if (ctype_isElips (c)) return FALSE;

  if (ctype_isComplex (c))
    {
      ctbase ctb = ctype_getCtbaseSafe (c);
      bool res = ctbase_isPointer (ctb);

      return res;
    }
  else
    {
      bool res = ctentry_isPointer (ctype_getCtentry (c));

      return res;
    }
}

/*
** ctype_isArray
**
** true for C and LCL array's
*/

bool
ctype_isArray (ctype c)
{
  if (ctype_isElips (c)) return FALSE;

  if (ctype_isComplex (c))
    return (ctbase_isEitherArray (ctype_getCtbaseSafe (c)));
  else
    return (ctentry_isArray (ctype_getCtentry (c)));
}

bool ctype_isIncompleteArray (ctype c)
{
  return (ctype_isArray (c) && !ctype_isFixedArray (c));
}

bool ctype_isFixedArray (ctype c)
{
  if (ctype_isElips (c)) return FALSE;

  return (ctbase_isFixedArray (ctype_getCtbaseSafe (c)));
}

bool
ctype_isArrayPtr (ctype c)
{
  return ((ctype_isArray (c)) || (ctype_isPointer (c)));
}

typeId
ctype_typeId (ctype c)
{
  return (ctbase_typeId (ctype_getCtbase (c)));
}

cstring
ctype_unparseDeclaration (ctype c, /*@only@*/ cstring name)
{
  llassert (!(ctype_isElips (c) || ctype_isMissingParamsMarker (c)));
  return (ctbase_unparseDeclaration (ctype_getCtbase (c), name));
}

cstring
ctype_unparse (ctype c)
{
  if (ctype_isElips (c))
    {
      return cstring_makeLiteralTemp ("...");
    }
  else if (ctype_isMissingParamsMarker (c))
    {
      return cstring_makeLiteralTemp ("-");
    }
  else
    {
      /*@-modobserver@*/
      return (ctentry_doUnparse (ctype_getCtentry (c)));
      /*@=modobserver@*/
    }
}
 
cstring
ctype_unparseSafe (ctype c)
{
  if (ctype_isElips (c))
    {
      return cstring_makeLiteralTemp ("...");
    }
  else if (ctype_isMissingParamsMarker (c))
    {
      return cstring_makeLiteralTemp ("-");
    }
  else
    {
      cstring ret;

      if /*@+enumint@*/ (c >= CTK_PLAIN && c < cttab.size) /*@=enumint@*/
	{
	  ctentry cte = ctype_getCtentry (c);
	  
	  if (cstring_isDefined (cte->unparse))
	    {
	      return (cte->unparse);
	    }
	}
      
      ret = message ("[%d]", (int) c);
      cstring_markOwned (ret);
      return ret;
    }
}

cstring
ctype_unparseDeep (ctype c)
{
  if (ctype_isElips (c))
    {
      return cstring_makeLiteralTemp ("...");
    }
  if (ctype_isMissingParamsMarker (c))
    {
      return cstring_makeLiteralTemp ("-");
    }
      
  return (ctentry_doUnparseDeep (ctype_getCtentry (c)));
}

ctype
ctype_undump (char **c)
{
  return ((ctype) getInt (c));	/* check its valid? */
}

cstring
ctype_dump (ctype c)
{
  DPRINTF (("Ctype dump: %s", ctype_unparse (c)));

  if (c < 0)
    {
      /* Handle invalid types in a kludgey way. */
      return (message ("0"));
    }
  
  if (ctype_isUA (c))
    {
      cstring tname = usymtab_getTypeEntryName 
	(usymtab_convertId (ctype_typeId (c)));
      
      if (cstring_equal (tname, context_getBoolName ()))
	{
	  cstring_free (tname);
	  return (message ("%d", ctype_bool));
	}
      
      cstring_free (tname);
    }

  DPRINTF (("Returning: %d", c));
  return (message ("%d", c));
}

ctype
ctype_getBaseType (ctype c)
{
  ctentry cte = ctype_getCtentry (c);

  switch (ctentry_getKind (cte))
    {
    case CTK_UNKNOWN:
      llcontbuglit ("ctype_getBaseType: unknown ctype"); break;
    case CTK_INVALID:
      llcontbuglit ("ctype_getBaseType: invalid ctype"); break;
    case CTK_PLAIN:
      return c;
    case CTK_PTR:
    case CTK_ARRAY:
      return (ctype_getBaseType (ctype_baseArrayPtr (c)));
    case CTK_COMPLEX:
      {
	ctbase ctb = cte->ctbase;

	if (ctbase_isDefined (ctb))
	  {
	    switch (ctb->type)
	      {
	      case CT_UNKNOWN:
	      case CT_PRIM:
	      case CT_USER:
	      case CT_ENUM:
	      case CT_ENUMLIST:
	      case CT_BOOL:
	      case CT_ABST:
	      case CT_FCN:
	      case CT_STRUCT:
	      case CT_UNION:
	      case CT_EXPFCN:
		return c;
	      case CT_PTR:
	      case CT_ARRAY:
		return (ctype_getBaseType (ctb->contents.base));
	      case CT_FIXEDARRAY:
		return (ctype_getBaseType (ctb->contents.farray->base));
	      case CT_CONJ:		/* base type of A conj branch? */
		return (ctype_getBaseType (ctb->contents.conj->a));
	      }
	  }
	else
	  {
	    return c;
	  }
      }
    default:
      llbuglit ("ctype_newBase: bad case");
    }
  llcontbuglit ("ctype_getBaseType: unreachable code");
  return ((ctype)NULL);
}

ctype
ctype_adjustPointers (int np, ctype c)
{
  
  if (ctype_isFunction (c))
    {
      c = ctype_makeParamsFunction
	(ctype_adjustPointers (np, ctype_returnValue (c)),
	 uentryList_copy (ctype_argsFunction (c)));
    }
  else
    {
      /* fix this should not use getBaseType ??? */
      ctype cb = ctype_getBaseType (c);

      while (np > 0)
	{
	  cb = ctype_makePointer (cb);
	  np--;
	}
      c = ctype_newBase (c, cb);
    }

  return (c);
}


enumNameList
ctype_elist (ctype c)
{
  return (ctbase_elist (ctype_getCtbase (c)));
}

bool
ctype_isFirstVoid (ctype c)
{
  return (c == CTX_VOID || (ctype_isConj (c) && ctype_isFirstVoid (ctype_getConjA (c))));
}

ctype
ctype_createEnum (/*@keep@*/ cstring tag, /*@keep@*/ enumNameList el)
{
  return (cttable_addComplex (ctbase_createEnum (tag, el)));
}

bool
ctype_isEnum (ctype c)
{
  return (ctype_isComplex (c) && ctbase_isEnum (ctype_getCtbase (c)));
}

cstring
ctype_enumTag (ctype c)
{
  llassert (ctype_isEnum (c));

  return (ctbase_enumTag (ctype_getCtbaseSafe (c)));
}

bool
ctype_isStruct (ctype c)
{
  return (ctype_isComplex (c) && ctbase_isStruct (ctype_getCtbaseSafe (c)));
}

bool
ctype_isUnion (ctype c)
{
  return (ctype_isComplex (c) && ctbase_isUnion (ctype_getCtbaseSafe (c)));
}

ctype
ctype_resolveNumerics (ctype c1, ctype c2)
{
  /*
  ** returns longest type of c1 and c2
  */

  if (c1 == c2) return c1;

  c1 = ctype_realType (c1);
  c2 = ctype_realType (c2);

  if (ctype_isEnum (c1)) c1 = ctype_unknown;
  if (ctype_isEnum (c2)) c2 = ctype_int;

  if (c1 == ctype_ldouble || c2 == ctype_ldouble) return ctype_ldouble;
  if (c1 == ctype_ulint || c2 == ctype_ulint) return ctype_ulint;
  if (c1 == ctype_lint || c2 == ctype_lint) return ctype_lint;
  if (c1 == ctype_uint || c2 == ctype_uint) return ctype_uint;
  if (c1 == ctype_int || c2 == ctype_int) return ctype_int;
  if (c1 == ctype_sint || c2 == ctype_sint) return ctype_sint;
  if (c1 == ctype_uchar || c2 == ctype_uchar) return ctype_uchar;
  if (c1 == ctype_char || c2 == ctype_char) return ctype_char;

  if (ctype_isKnown (c1)) return c1;
  else return c2;
}

bool
ctype_isStructorUnion (ctype c)
{
  return (ctype_isStruct (c) || ctype_isUnion (c));
}

ctype
ctype_fixArrayPtr (ctype c)
{
  if (ctype_isArray (c))
    {
      return (ctype_makePointer (ctype_baseArrayPtr (c)));
    }
  else
    return c;
}

/*
** createUnnamedStruct/Union
**
** check if it corresponds to an existing LCL-specified unnamed struct
** otherwise, give it a new tag
*/

ctype
ctype_createUnnamedStruct (/*@only@*/ uentryList f)
{
  ctype ret = usymtab_structFieldsType (f);

  if (ctype_isDefined (ret))
    {
      uentryList_free (f);
      return ret;
    }
  else
    {
      cstring ft = fakeTag ();
      ctype ct = ctype_createStruct (cstring_copy (ft), f);
      uentry ue = uentry_makeStructTagLoc (ft, ct);
      
      usymtab_supGlobalEntry (ue);
      
      cstring_free (ft);
      return (ct);
    }
}

ctype
ctype_createUnnamedUnion (/*@only@*/ uentryList f)
{
  ctype ret = usymtab_unionFieldsType (f);
  
  if (ctype_isDefined (ret))
    {
      uentryList_free (f);
      return ret;
    }
  else
    {
      cstring ft = fakeTag ();
      ctype ct = ctype_createUnion (cstring_copy (ft), f);
      uentry ue = uentry_makeUnionTagLoc (ft, ct);

      usymtab_supGlobalEntry (ue);
      cstring_free (ft);
      return (ct);
    }
}

ctype
ctype_createForwardStruct (cstring n)
{
  uentry ue  = uentry_makeStructTag (n, ctype_unknown, fileloc_undefined);
  ctype ct = usymtab_supForwardTypeEntry (ue);

  cstring_free (n);
  return (ct);
}

ctype
ctype_createForwardUnion (cstring n)
{
  uentry ue  = uentry_makeUnionTag (n, ctype_unknown, fileloc_undefined);
  ctype ct = usymtab_supForwardTypeEntry (ue);

  cstring_free (n);
  return (ct);
}

ctype
ctype_removePointers (ctype c)
{
  ctype oldc;

  while (ctype_isArrayPtr (c))
    {
      oldc = c;
      c = ctype_baseArrayPtr (c);
      llassert (c != oldc);
    }
  
  return (c);
}

bool ctype_isMutable (ctype t)
{
  if (ctype_isUA (t))
    {
      return (uentry_isMutableDatatype 
	      (usymtab_getTypeEntry (ctype_typeId (t))));
    }
  else 
    {
      return (ctype_isPointer (ctype_realType (t)));
    }
}

bool ctype_isRefCounted (ctype t)
{
  if (ctype_isUA (t))
    {
      return (uentry_isRefCountedDatatype 
	      (usymtab_getTypeEntry (ctype_typeId (t))));
    }

  return FALSE;
}

bool ctype_isVisiblySharable (ctype t)
{
  if (ctype_isUnknown (t)) return TRUE;

  if (ctype_isConj (t))
    {
      return (ctype_isVisiblySharable (ctype_getConjA (t))
	      || ctype_isVisiblySharable (ctype_getConjB (t)));
    }

  if (ctype_isMutable (t))
    {
      if (ctype_isUA (t))
	{
	  ctype rt = ctype_realType (t);

	  if (rt == t)
	    {
	      return TRUE;
	    }
	  else
	    {
	      return ctype_isVisiblySharable (rt);
	    }
	}
      else
	{
	  return TRUE;
	}
    }
  
  return FALSE;
}

# if 0
/* Replaced by ctype_isMutable (more sensible) */
bool ctype_canAlias (ctype ct)
{
  /* can ct refer to memory locations?
  **       ==> a pointer or a mutable abstract type
  **           arrays?  
  */

  ctype tr = ctype_realType (ct);

  return (ctype_isPointer (tr) || ctype_isMutable (ct) || ctype_isStructorUnion (tr));
}
# endif

/*
** c1 is the dominant type; c2 is the modifier type
**
** eg. double + long int => long double
*/

ctype ctype_combine (ctype dominant, ctype modifier)
{
  DPRINTF (("Combine: %s + %s", 
	    ctype_unparse (dominant),
	    ctype_unparse (modifier)));

  if (ctype_isConj (dominant)) 
    {      
      ctype res;

      if (ctype_isExplicitConj (dominant))
	{
	  res = ctype_makeExplicitConj (ctype_combine (ctype_getConjA (dominant), 
						       modifier),
					ctype_getConjB (dominant));
	}
      else
	{
	  res = ctype_makeConj (ctype_combine (ctype_getConjA (dominant),
					       modifier),
				ctype_getConjB (dominant));
	}

      return res;
    }

  if (ctype_isUnknown (modifier)) 
    {
      return dominant;
    }
  else if (ctype_isUnknown (dominant))
    {
      return modifier; 
    }
  else
    {
      if (ctype_isEnum (dominant)) dominant = ctype_int;
      if (ctype_isEnum (modifier)) modifier = ctype_int;
      
      if (modifier == ctype_uint)
	{
	  if (dominant == ctype_int) return ctype_uint;
	  if (dominant == ctype_lint) return ctype_ulint;
	  if (dominant == ctype_sint) return ctype_usint;
	  if (dominant == ctype_char) return ctype_uchar;

	  /* evs 2000-07-28: added this line */
	  if (dominant == ctype_llint) return ctype_ullint;

	  if ((dominant == ctype_uint) || dominant == ctype_uchar)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate unsigned qualifier"),
			    g_currentloc);

	      return ctype_uint;
	    }
	  else
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Type qualifier unsigned used with %s", 
				     ctype_unparse (dominant)),
			    g_currentloc);
	  
	      return dominant;
	    }
	}
      else if (modifier == ctype_llint)
	{
	  if (dominant == ctype_int)
	    {
	      return ctype_llint;
	    }
	  
	  voptgenerror (FLG_DUPLICATEQUALS, 
			message ("Duplicate long qualifier on non-int"),
			g_currentloc);
	}
      else if (modifier == ctype_lint)
	{
	  if (dominant == ctype_int) return ctype_lint;
	  if (dominant == ctype_uint) return ctype_ulint;
	  if (dominant == ctype_double) return ctype_ldouble;
	  
	  if (dominant == ctype_lint || dominant == ctype_ulint 
	      || dominant == ctype_sint || dominant == ctype_usint
	      || dominant == ctype_ldouble)
	    {
	      if (dominant == ctype_lint)
		{
		  /* long long not supported by ANSI */
		  return ctype_llint;
		}
	      
	      if (dominant == ctype_sint || dominant == ctype_usint)
		{
		  if (!context_getFlag (FLG_IGNOREQUALS))
		    {
		      llerrorlit (FLG_SYNTAX, 
				  "Contradictory long and short type qualifiers");
		    }
		}
	      else
		{
		  voptgenerror (FLG_DUPLICATEQUALS, 
				message ("Duplicate long qualifier"),
				g_currentloc);
		}
	      
	      return ctype_lint;
	    }
	}
      else if (modifier == ctype_sint)
	{
	  if (dominant == ctype_int) return ctype_sint;
	  if (dominant == ctype_uint) return ctype_usint;
	  
	  if (dominant == ctype_sint || dominant == ctype_usint)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate short qualifier"),
			    g_currentloc);
	      return ctype_uint;
	    }
	  else if (dominant == ctype_lint)
	    {
	      if (!context_getFlag (FLG_IGNOREQUALS))
		{
		  llerrorlit (FLG_SYNTAX, 
			      "Contradictory long and short type qualifiers");
		}
	      
	      return dominant;
	    }
	  else
	    {
	      if (!context_getFlag (FLG_IGNOREQUALS))
		{
		  llerror (FLG_SYNTAX, 
			   message ("Type qualifier short used with %s", 
				    ctype_unparse (dominant)));
		}

	      return dominant;
	    }
	}
      else if (modifier == ctype_ulint)
	{
	  if (dominant == ctype_int) return modifier;
	  
	  if (dominant == ctype_lint || dominant == ctype_ulint)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate long qualifier"),
			    g_currentloc);

	      return modifier;
	    }
	  
	  if (dominant == ctype_uint || dominant == ctype_usint)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate unsigned qualifier"),
			    g_currentloc);

	      return modifier;
	    }
	  
	  if (dominant == ctype_sint || dominant == ctype_usint)
	    {
	      if (!context_getFlag (FLG_IGNOREQUALS))
		{
		  llerrorlit (FLG_SYNTAX,
			      "Contradictory long and short type qualifiers");
		}

	      return dominant;
	    }
	  
	  if (!context_getFlag (FLG_IGNOREQUALS))
	    {
	      llerror (FLG_SYNTAX,
		       message ("Type qualifiers unsigned long used with %s", 
				ctype_unparse (dominant)));
	    }

	  return dominant;
	}
      else if (modifier == ctype_usint)
	{
	  if (dominant == ctype_int) return modifier;
	  
	  if (dominant == ctype_sint || dominant == ctype_usint)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate short qualifier"),
			    g_currentloc);
	      return modifier;
	    }
	  
	  if (dominant == ctype_uint)
	    {
	      voptgenerror (FLG_DUPLICATEQUALS, 
			    message ("Duplicate unsigned qualifier"),
			    g_currentloc);

	      return modifier;
	    }
	  
	  if (dominant == ctype_lint || dominant == ctype_ulint
	      || dominant == ctype_llint)
	    {
	      if (!context_getFlag (FLG_IGNOREQUALS))
		{
		  llerrorlit (FLG_SYNTAX, 
			      "Contradictory long and short type qualifiers");
		}

	      return dominant;
	    }
	  
	  if (!context_getFlag (FLG_IGNOREQUALS))
	    {
	      llerror (FLG_SYNTAX, 
		       message ("Type qualifiers unsigned short used with %s",
				ctype_unparse (dominant)));
	    }

	  return dominant;
	}
      else
	{
	  ;
	}

      return dominant;
    }
}
  
ctype ctype_resolve (ctype c)
{
  if (ctype_isUnknown (c)) return ctype_int;
  return c;
}

ctype ctype_fromQual (qual q)
{
  if (qual_isSigned (q)) return ctype_int;
  if (qual_isUnsigned (q)) return ctype_uint;
  if (qual_isLong (q)) return ctype_lint;
  if (qual_isShort (q)) return ctype_sint;
  
  llcontbug (message ("ctype_fromQual: invalid qualifier: %s", qual_unparse (q)));
  return ctype_unknown;
}

bool 
ctype_isAnyFloat (ctype c)
{
  return (cprim_isAnyReal (ctype_toCprim (c)));
}

bool
ctype_isUnsigned (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isUnsigned (ctype_getConjA (c)) ||
	    ctype_isUnsigned (ctype_getConjB (c)));

  return (c == ctype_uint || c == ctype_uchar
	  || c == ctype_usint || c == ctype_ulint
	  || c == ctype_unsignedintegral);
}

static bool
ctype_isLong (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isLong (ctype_getConjA (c)) ||
	    ctype_isLong (ctype_getConjB (c)));

  return (c == ctype_lint || c == ctype_ulint);
}

static bool
ctype_isShort (ctype c)
{
  if (ctype_isConj (c))
    return (ctype_isShort (ctype_getConjA (c)) ||
	    ctype_isShort (ctype_getConjB (c)));

  return (c == ctype_sint || c == ctype_usint);
}

bool
ctype_isStackAllocated (ctype c)
{
  ctype ct = ctype_realType (c);

  if (ctype_isConj (ct))
    return (ctype_isStackAllocated (ctype_getConjA (ct)) ||
	    ctype_isStackAllocated (ctype_getConjB (ct)));
  
  return (ctype_isArray (c) || ctype_isSU (c));
}

static bool ctype_isMoreUnsigned (ctype c1, ctype c2)
{
  return (ctype_isUnsigned (c1) && !ctype_isUnsigned (c2));
}

static bool ctype_isLonger (ctype c1, ctype c2)
{
  return ((ctype_isDouble (c1) && !ctype_isDouble (c2))
	  || (ctype_isLong (c1) && !ctype_isLong (c2))
	  || (ctype_isShort (c2) && !ctype_isShort (c1)));
}

ctype
ctype_widest (ctype c1, ctype c2)
{
  if (ctype_isMoreUnsigned (c2, c1)
      || ctype_isLonger (c2, c1))
    {
      return c2;
    }
  else
    {
      return c1;
    }
}

/*drl 11/28/2000 */
/* requires that the type is an fixed array */
/* return the size of the array */

long int ctype_getArraySize (ctype c)
{
  ctentry cte = ctype_getCtentry (c);
  ctbase ctb;
  llassert ( (ctentry_getKind (cte) ==  CTK_COMPLEX) || (ctentry_getKind(cte) == CTK_ARRAY) );

  ctb = cte->ctbase;

  llassert (ctbase_isDefined (ctb) );
  
  llassert (ctb->type == CT_FIXEDARRAY);

  return (ctb->contents.farray->size);
}