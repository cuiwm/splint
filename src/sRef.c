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
** storeRef.c
**
** Memory management:
**    storeRef's are kept in allRefs for each function scope, and all are
**    free'd at the end of the function.  This relies on the constraint that
**    no storeRef created while checking a function is used outside that
**    function.
**
**    storeRefs in the file and global scopes are free'd by the uentry.
**
*/

# include "lclintMacros.nf"
# include "basic.h"
# include "exprChecks.h"
# include "aliasChecks.h"
# include "sRefTable.h"
# include "structNames.h"

/*
** Predicate functions that evaluate both arguments in order.
*/

/*@notfunction@*/
# define OR(a,b)  (a ? (b, TRUE) : b)

/*@notfunction@*/
# define AND(a,b) (a ? b : (b, FALSE))

static bool sRef_isDerived (sRef p_s) /*@*/ ;

static /*@exposed@*/ sRef sRef_fixDirectBase (sRef p_s, sRef p_base) 
   /*@modifies p_base@*/ ;

static bool sRef_isAllocatedStorage (sRef p_s) /*@*/ ;
static void sRef_setNullErrorLoc (sRef p_s, fileloc) /*@*/ ;

static void
  sRef_aliasSetComplete (void (p_predf) (sRef, fileloc), sRef p_s, fileloc p_loc)
  /*@modifies p_s@*/ ;

static int sRef_depth (sRef p_s) /*@*/ ;

static void
  sRef_innerAliasSetComplete (void (p_predf) (sRef, fileloc), sRef p_s, 
			      fileloc p_loc)
  /*@modifies p_s@*/ ;

static void
  sRef_innerAliasSetCompleteParam (void (p_predf) (sRef, sRef), sRef p_s, sRef p_t)
  /*@modifies p_s@*/ ;

static void
  sRef_aliasSetCompleteParam (void (p_predf) (sRef, alkind, fileloc), sRef p_s, 
			      alkind p_kind, fileloc p_loc)
  /*@modifies p_s@*/ ;

static speckind speckind_fromInt (int p_i);
static bool sRef_equivalent (sRef p_s1, sRef p_s2);
static bool sRef_isDeepUnionField (sRef p_s);
static void sRef_addDeriv (/*@notnull@*/ sRef p_s, /*@notnull@*/ sRef p_t);
static sRef sRef_makeResultType (ctype p_ct) /*@*/ ;
static bool sRef_checkModify (sRef p_s, sRefSet p_sl) /*@*/ ;

static bool skind_isSimple (skind sk)
{
  switch (sk)
    {
    case SK_PARAM: case SK_CVAR: case SK_CONST:
    case SK_OBJECT: case SK_UNKNOWN: case SK_NEW:
      return TRUE;
    default:
      return FALSE;
    }
}

static void sinfo_free (/*@special@*/ /*@temp@*/ /*@notnull@*/ sRef p_s)
   /*@uses p_s->kind, p_s->info@*/
   /*@releases p_s->info@*/ ;

static /*@null@*/ sinfo sinfo_copy (/*@notnull@*/ sRef p_s) /*@*/ ;
static void sRef_setPartsFromUentry (sRef p_s, uentry p_ue)
   /*@modifies p_s@*/ ;
static bool checkDeadState (/*@notnull@*/ sRef p_el, bool p_tbranch, fileloc p_loc);
static sRef sRef_constructPointerAux (/*@notnull@*/ /*@exposed@*/ sRef p_t) /*@*/ ;

static void 
  sRef_combineExKinds (/*@notnull@*/ sRef p_res, /*@notnull@*/ sRef p_other)
  /*@modifies p_res@*/ ;

static void 
  sRef_combineAliasKinds (/*@notnull@*/ sRef p_res, /*@notnull@*/ sRef p_other, 
			  clause p_cl, fileloc p_loc)
  /*@modifies p_res@*/ ;

static void
  sRef_combineNullState (/*@notnull@*/ sRef p_res, /*@notnull@*/ sRef p_other)
  /*@modifies p_res@*/ ;

static void
  sRef_combineDefState (/*@notnull@*/ sRef p_res, /*@notnull@*/ sRef p_other)
  /*@modifies p_res@*/ ;

static void sRef_setStateFromAbstractUentry (sRef p_s, uentry p_ue) 
  /*@modifies p_s@*/ ;

static void 
  sinfo_update (/*@notnull@*/ /*@exposed@*/ sRef p_res, 
		/*@notnull@*/ /*@exposed@*/ sRef p_other);
static /*@only@*/ alinfo alinfo_makeRefLoc (/*@exposed@*/ sRef p_ref, fileloc p_loc);
static void sRef_setDefinedAux (sRef p_s, fileloc p_loc, bool p_clear)
   /*@modifies p_s@*/ ;
static void sRef_setDefinedNoClear (sRef p_s, fileloc p_loc)
   /*@modifies p_s@*/ ;
static void sRef_setStateAux (sRef p_s, sstate p_ss, fileloc p_loc)
   /*@modifies p_s@*/;

static /*@exposed@*/ sRef 
  sRef_buildNCField (/*@exposed@*/ sRef p_rec, /*@exposed@*/ cstring p_f);

static void 
  sRef_mergeStateAux (/*@notnull@*/ sRef p_res, /*@notnull@*/ sRef p_other, 
		      clause p_cl, bool p_opt, fileloc p_loc,
		      bool p_doDerivs)
  /*@modifies p_res, p_other@*/ ;

static /*@null@*/ sinfo sinfo_fullCopy (/*@notnull@*/ sRef p_s);
static bool sRef_doModify (sRef p_s, sRefSet p_sl) /*@modifies p_s@*/ ;
static bool sRef_doModifyVal (sRef p_s, sRefSet p_sl) /*@modifies p_s@*/;
static bool sRef_checkModifyVal (sRef p_s, sRefSet p_sl) /*@*/ ;

static /*@only@*/ sRefSet
  sRef_mergeDerivs (/*@only@*/ sRefSet p_res, sRefSet p_other, 
		    bool p_opt, clause p_cl, fileloc p_loc);

static /*@only@*/ sRefSet
  sRef_mergeUnionDerivs (/*@only@*/ sRefSet p_res, 
			 /*@exposed@*/ sRefSet p_other,
			 bool p_opt, clause p_cl, fileloc p_loc);

static /*@only@*/ sRefSet 
  sRef_mergePdefinedDerivs (/*@only@*/ sRefSet p_res, sRefSet p_other, bool p_opt,
			    clause p_cl, fileloc p_loc);

static /*@only@*/ cstring sRef_unparseWithArgs (sRef p_s, uentryList p_args);
static /*@only@*/ cstring sRef_unparseNoArgs (sRef p_s);

static /*@exposed@*/ sRef sRef_findDerivedPointer (sRef p_s);
static /*@exposed@*/ sRef sRef_findDerivedField (/*@notnull@*/ sRef p_rec, cstring p_f);
static /*@exposed@*/ sRef
  sRef_getDeriv (/*@notnull@*/ /*@returned@*/ sRef p_set, sRef p_guide);

static bool inFunction = FALSE;
static /*@only@*/ sRefTable allRefs;

/* # define DEBUGREFS  */

# ifdef DEBUGREFS
static nsrefs = 0;
static totnsrefs = 0;
static maxnsrefs = 0;
static ntotrefers = 0;
static nrefers = 0;
# endif

/*@constant null alinfo alinfo_undefined; @*/
# define alinfo_undefined ((alinfo) NULL)

static /*@only@*/ /*@notnull@*/ alinfo alinfo_makeLoc (fileloc p_loc);
static /*@only@*/ alinfo alinfo_copy (alinfo p_a);

static /*@checked@*/ bool protectDerivs = FALSE;

void sRef_protectDerivs (void) /*@modifies protectDerivs@*/
{
  llassert (!protectDerivs);
  protectDerivs = TRUE;
}

void sRef_clearProtectDerivs (void) /*@modifies protectDerivs@*/
{
  llassert (protectDerivs);
  protectDerivs = FALSE;
}

/*
** hmmm...here be kind of a hack.  This function mysteriously appeared
** in my code, but I'm sure I didn't write it.
*/

bool
sRef_isRecursiveField (sRef s)
{
  if (sRef_isField (s))
    {
      if (sRef_depth (s) > 13)
	{
	  sRef base;
	  cstring fieldname;
	  
	  fieldname = sRef_getField (s);
	  base = sRef_getBase (s);
	  
	  while (sRef_isValid (base))
	    {
	      if (sRef_isField (base))
		{
		  if (cstring_equal (fieldname, sRef_getField (base)))
		    {
		      return TRUE;
		    }
		}
	      
	      base = sRef_getBaseSafe (base);
	    }
	}
    }

  return FALSE;
}

static void
sRef_addDeriv (/*@notnull@*/ sRef s, /*@notnull@*/ sRef t)
{
  if (!context_inProtectVars () 
      && !protectDerivs
      && sRef_isValid (s)
      && sRef_isValid (t)
      && !sRef_isConst (s))
    {
      int sd = sRef_depth (s);
      int td = sRef_depth (t);
      
      if (sd >= td)
	{
	  return;
	}

      if (sRef_isGlobal (s))
	{
	  if (context_inFunctionLike () 
	      && ctype_isKnown (sRef_getType (s))
	      && !ctype_isFunction (sRef_getType (s)))
	    {
	      globSet g = context_getUsedGlobs ();

	      if (!globSet_member (g, s))
		{
		  /* 
		  ** don't report as a bug 
		  ** 

		  llcontbug 
			(message ("sRef_addDeriv: global variable not in used "
				  "globs: %q / %s / %q",
				  sRef_unparse (s), 
				  ctype_unparse (sRef_getType (s)),
				  sRefSet_unparse (s->deriv)));
		  */
		}
	      else
		{
		  s->deriv = sRefSet_insert (s->deriv, t);
		}
	    }
	}
      else
	{
	  s->deriv = sRefSet_insert (s->deriv, t);
	}
    }
}

bool
sRef_deepPred (bool (predf) (sRef), sRef s)
{
  if (sRef_isValid (s))
    {
      if ((*predf)(s)) return TRUE;

      switch  (s->kind)
	{
	case SK_PTR:
	  return (sRef_deepPred (predf, s->info->ref));
	case SK_ARRAYFETCH:
	  return (sRef_deepPred (predf, s->info->arrayfetch->arr));
	case SK_FIELD:
	  return (sRef_deepPred (predf, s->info->field->rec));
	case SK_CONJ:
	  return (sRef_deepPred (predf, s->info->conj->a)
		  || sRef_deepPred (predf, s->info->conj->b));
	default:
	  return FALSE;
	}
    }

  return FALSE;
}

bool sRef_modInFunction (void)
{
  return inFunction;
}

void sRef_setStateFromType (sRef s, ctype ct)
{
  if (sRef_isValid (s))
    {
      if (ctype_isUser (ct))
	{
	  sRef_setStateFromUentry 
	    (s, usymtab_getTypeEntry (ctype_typeId (ct)));
	}
      else if (ctype_isAbstract (ct))
	{
	  sRef_setStateFromAbstractUentry 
	    (s, usymtab_getTypeEntry (ctype_typeId (ct)));
	}
      else
	{
	  ; /* not a user type */
	}
    }
}

static void sRef_setTypeState (sRef s)
{
  if (sRef_isValid (s))
    {
      sRef_setStateFromType (s, s->type);
    }
}

static void alinfo_free (/*@only@*/ alinfo a)
{
  if (a != NULL)
    {
      fileloc_free (a->loc);
      sfree (a);
    }
}

static /*@only@*/ alinfo alinfo_update (/*@only@*/ alinfo old, alinfo newinfo)
/*
** returns an alinfo with the same value as new.  May reuse the
** storage of old.  (i.e., same effect as copy, but more
** efficient.)
*/
{
  if (old == NULL) 
    {
      old = alinfo_copy (newinfo);
    }
  else if (newinfo == NULL)
    {
      alinfo_free (old);
      return NULL;
    }
  else
    {
      old->loc = fileloc_update (old->loc, newinfo->loc);
      old->ref = newinfo->ref;
      old->ue = newinfo->ue;
    }

  return old;
}

static /*@only@*/ alinfo alinfo_updateLoc (/*@only@*/ alinfo old, fileloc loc)
{
  if (old == NULL) 
    {
      old = alinfo_makeLoc (loc);
    }
  else
    {
      old->loc = fileloc_update (old->loc, loc);
      old->ue = uentry_undefined;
      old->ref = sRef_undefined;
    }

  return old;
}

static /*@only@*/ alinfo 
  alinfo_updateRefLoc (/*@only@*/ alinfo old, /*@dependent@*/ sRef ref, fileloc loc)
{
  if (old == NULL) 
    {
      old = alinfo_makeRefLoc (ref, loc);
    }
  else
    {
      old->loc = fileloc_update (old->loc, loc);
      old->ue = uentry_undefined;
      old->ref = ref;
    }

  return old;
}

static /*@only@*/ alinfo
alinfo_copy (alinfo a)
{
  if (a == NULL)
    {
      return NULL;
    }
  else
    {
      alinfo ret = (alinfo) dmalloc (sizeof (*ret));
      
      ret->loc = fileloc_copy (a->loc); /*< should report bug without copy! >*/
      ret->ue = a->ue;
      ret->ref = a->ref;

      return ret;
    }
}

static bool
  sRef_hasAliasInfoLoc (sRef s)
{
  return (sRef_isValid (s) && (s->aliasinfo != NULL)
	  && (fileloc_isDefined (s->aliasinfo->loc)));
}

static /*@falsenull@*/ bool
sRef_hasStateInfoLoc (sRef s)
{
  return (sRef_isValid (s) && (s->definfo != NULL) 
	  && (fileloc_isDefined (s->definfo->loc)));
}

static /*@falsenull@*/ bool
sRef_hasExpInfoLoc (sRef s)
{
  return (sRef_isValid (s) 
	  && (s->expinfo != NULL) && (fileloc_isDefined (s->expinfo->loc)));
}

static bool
sRef_hasNullInfoLoc (sRef s)
{
  return (sRef_isValid (s) && (s->nullinfo != NULL) 
	  && (fileloc_isDefined (s->nullinfo->loc)));
}

bool
sRef_hasAliasInfoRef (sRef s)
{
  return (sRef_isValid (s) && (s->aliasinfo != NULL) 
	  && (sRef_isValid (s->aliasinfo->ref)));
}

static /*@observer@*/ fileloc
sRef_getAliasInfoLoc (/*@exposed@*/ sRef s)
{
  llassert (sRef_isValid (s) && s->aliasinfo != NULL
	    && (fileloc_isDefined (s->aliasinfo->loc)));
  return (s->aliasinfo->loc);
}

static /*@observer@*/ fileloc
sRef_getStateInfoLoc (/*@exposed@*/ sRef s)
{
  llassert (sRef_isValid (s) && s->definfo != NULL 
	    && (fileloc_isDefined (s->definfo->loc)));
  return (s->definfo->loc);
}

static /*@observer@*/ fileloc
sRef_getExpInfoLoc (/*@exposed@*/ sRef s)
{
  llassert (sRef_isValid (s) && s->expinfo != NULL 
	    && (fileloc_isDefined (s->expinfo->loc)));
  return (s->expinfo->loc);
}

static /*@observer@*/ fileloc
sRef_getNullInfoLoc (/*@exposed@*/ sRef s)
{
  llassert (sRef_isValid (s) && s->nullinfo != NULL 
	    && (fileloc_isDefined (s->nullinfo->loc)));
  return (s->nullinfo->loc);
}

/*@observer@*/ sRef
  sRef_getAliasInfoRef (/*@exposed@*/ sRef s)
{
  llassert (sRef_isValid (s) && s->aliasinfo != NULL);
  return (s->aliasinfo->ref);
}

static /*@only@*/ /*@notnull@*/ alinfo
alinfo_makeLoc (fileloc loc)
{
  alinfo ret = (alinfo) dmalloc (sizeof (*ret));

  ret->loc = fileloc_copy (loc); /* don't need to copy! */
  ret->ue = uentry_undefined;
  ret->ref = sRef_undefined;
  
  return ret;
}

static /*@only@*/ alinfo
alinfo_makeRefLoc (/*@exposed@*/ sRef ref, fileloc loc)
{
  alinfo ret = (alinfo) dmalloc (sizeof (*ret));

  ret->loc = fileloc_copy (loc);
  ret->ref = ref;
  ret->ue  = uentry_undefined;
  
  return ret;
}

/*
** This function should be called before new sRefs are created
** somewhere where they will have a lifetime greater than the
** current function scope.
*/

void sRef_setGlobalScope ()
{
  llassert (inFunction);
  inFunction = FALSE;
}

void sRef_clearGlobalScope ()
{
  llassert (!inFunction);
  inFunction = TRUE;
}

static bool oldInFunction = FALSE;

void sRef_setGlobalScopeSafe ()
{
    oldInFunction = inFunction;
  inFunction = FALSE;
}

void sRef_clearGlobalScopeSafe ()
{
    inFunction = oldInFunction;
}

void sRef_enterFunctionScope ()
{
  llassert (!inFunction);
  llassert (sRefTable_isEmpty (allRefs));
  inFunction = TRUE;
}

void sRef_exitFunctionScope ()
{
  
  if (inFunction)
    {
      sRefTable_clear (allRefs);
      inFunction = FALSE;
    }
  else
    {
      llbuglit ("sRef_exitFunctionScope: not in function");
    }
}
  
void sRef_destroyMod () /*@globals killed allRefs;@*/
{
# ifdef DEBUGREFS  
  llmsg (message ("Live: %d / %d ", nsrefs, totnsrefs));  
# endif

  sRefTable_free (allRefs);
}

/*
** Result of sRef_alloc is dependent since allRefs may
** reference it.  It is only if !inFunction.
*/

static /*@dependent@*/ /*@out@*/ /*@notnull@*/ sRef
sRef_alloc (void)
{
  sRef s = (sRef) dmalloc (sizeof (*s));

  if (inFunction)
    {
      allRefs = sRefTable_add (allRefs, s);
      /*@-branchstate@*/ 
    } 
  /*@=branchstate@*/

# ifdef DEBUGREFS
  if (nsrefs >= maxnsrefs)
    {
      maxnsrefs = nsrefs;
    }

  totnsrefs++;
  nsrefs++;
# endif

  /*@-mustfree@*/ /*@-freshtrans@*/
  return s;
  /*@=mustfree@*/ /*@=freshtrans@*/
}

static /*@dependent@*/ /*@notnull@*/ /*@special@*/ sRef
  sRef_new (void)
  /*@defines result@*/
  /*@post:isnull result->aliasinfo, result->definfo, result->nullinfo, 
                 result->expinfo, result->info, result->deriv@*/
{
  sRef s = sRef_alloc ();

  s->kind = SK_UNKNOWN;
  s->safe = TRUE;
  s->modified = FALSE;
  s->type = ctype_unknown;
  s->defstate = SS_UNKNOWN;

  /* start modifications */
  s->bufinfo.bufstate = BB_NOTNULLTERMINATED;
  /* end modifications */

  s->aliaskind = AK_UNKNOWN;
  s->oaliaskind = AK_UNKNOWN;

  s->nullstate = NS_UNKNOWN;

  s->expkind = XO_UNKNOWN;
  s->oexpkind = XO_UNKNOWN;

  s->aliasinfo = alinfo_undefined;
  s->definfo = alinfo_undefined;
  s->nullinfo = alinfo_undefined;
  s->expinfo = alinfo_undefined;

  s->info = NULL;
  s->deriv = sRefSet_undefined;

  return s;
}

static /*@notnull@*/ /*@exposed@*/ sRef
sRef_fixConj (/*@notnull@*/ sRef s)
{
  if (sRef_isConj (s))
    {
      do {
	s = sRef_getConjA (s);
      } while (sRef_isConj (s));
      
      llassert (sRef_isValid (s));
      return s; /* don't need to ref */
    }
  else
    {
      return s;
    }
}

static bool 
sRef_isExternallyVisibleAux (sRef s)
{
  bool res = FALSE;
  sRef base = sRef_getRootBase (s);

  if (sRef_isValid (base))
    {
      res = sRef_isParam (base) || sRef_isGlobal (base) || sRef_isExternal (base);
    }

  return res;
}

bool 
  sRef_isExternallyVisible (sRef s)
{
  return (sRef_aliasCheckSimplePred (sRef_isExternallyVisibleAux, s));
}

/*@exposed@*/ uentry
sRef_getBaseUentry (sRef s)
{
  sRef base = sRef_getRootBase (s);
  uentry res = uentry_undefined;
  
  if (sRef_isValid (base))
    {
      switch (base->kind)
	{
	case SK_PARAM:
	  res = usymtab_getRefQuiet (paramsScope, base->info->paramno);
	  break;

	case SK_CVAR:
	  res = usymtab_getRefQuiet (base->info->cvar->lexlevel, 
				     base->info->cvar->index);
	  break;

	default:
	  break;
	}  
    }

  return res;
}

/*
** lookup the current uentry corresponding to s, and return the corresponding sRef.
** yuk yuk yuk yuk yuk yuk yuk yuk
*/

/*@exposed@*/ sRef
sRef_updateSref (sRef s)
{
  sRef inner;
  sRef ret;
  sRef res;

  if (!sRef_isValid (s)) return sRef_undefined;

  
  switch (s->kind)
    {
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_NEW:
    case SK_TYPE:
    case SK_EXTERNAL:
    case SK_DERIVED:
    case SK_UNCONSTRAINED:
    case SK_CONST:
    case SK_SPECIAL:
    case SK_RESULT:
      return s; 
    case SK_PARAM:
    case SK_CVAR:
      {
	uentry ue = sRef_getUentry (s);

	/* must be raw name!  (need the marker) */
	ue = usymtab_lookupSafe (uentry_rawName (ue));
	
	if (uentry_isUndefined (ue))
	  {
	    	    return s;
	  }
	else
	  {
	    	    return (uentry_getSref (ue));
	  }
      }
    case SK_ARRAYFETCH:
      /* special case if ind known */
      inner = s->info->arrayfetch->arr;
      ret = sRef_updateSref (inner);

      if (ret == inner) 
	{
	  res = s; 
	}
      else 
	{
	  res = sRef_makeArrayFetch (ret);
	}

      return res;

    case SK_FIELD:
      inner = s->info->field->rec;
      ret = sRef_updateSref (inner);

      if (ret == inner) 
	{
	  res = s; 
	}
      else 
	{
	  res = (sRef_makeField (ret, s->info->field->field));
	}

      return (res);
    case SK_PTR:
      inner = s->info->ref;
      ret = sRef_updateSref (inner);
      if (ret == inner) 
	{
	  res = s; 
	}
      else
	{
	  res = sRef_makePointer (ret);
	}

      return (res);

    case SK_ADR:
      inner = s->info->ref;
      ret = sRef_updateSref (inner);

      if (ret == inner)
	{
	  res = s; 
	}
      else 
	{
	  res = sRef_makeAddress (ret);
	}

      return (res);

    case SK_CONJ:
      {
	sRef innera = s->info->conj->a;
	sRef innerb = s->info->conj->b;
	sRef reta = sRef_updateSref (innera);
	sRef retb = sRef_updateSref (innerb);

	if (innera == reta && innerb == retb)
	  {
	    res = s;
	  }
	else 
	  {
	    res = sRef_makeConj (reta, retb);
	  }

	return (res);
      }
    }
  
  BADEXIT;
}

uentry
sRef_getUentry (sRef s)
{
  llassert (sRef_isValid (s));

  switch (s->kind)
    {
    case SK_PARAM:
      return (usymtab_getRefQuiet (paramsScope, s->info->paramno));
    case SK_CVAR:
      return (usymtab_getRefQuiet (s->info->cvar->lexlevel, s->info->cvar->index));
    case SK_CONJ:
      {
	if (sRef_isCvar (s->info->conj->a) || sRef_isParam (s->info->conj->a)
	    || sRef_isConj (s->info->conj->a))
	  {
	    return sRef_getUentry (s->info->conj->a);
	  }
	else 
	  {
	    return sRef_getUentry (s->info->conj->b);
	  }
      }
    case SK_UNKNOWN:
    case SK_SPECIAL:
      return uentry_undefined;
    BADDEFAULT;
    }
}

int
sRef_getParam (sRef s)
{
  llassert (sRef_isValid (s));
  llassert (s->kind == SK_PARAM);

  return s->info->paramno;
}

bool
sRef_isModified (sRef s)
{
    return (!sRef_isValid (s) || s->modified);
}

void sRef_setModified (sRef s)
{
  if (sRef_isValid (s))
    {
      s->modified = TRUE;

      
      if (sRef_isRefsField (s))
	{
	  sRef base = sRef_getBase (s);

	  
	  llassert (s->kind == SK_FIELD);

	  
	  if (sRef_isPointer (base))
	    {
	      base = sRef_getBase (base);
	      	    }

	  if (sRef_isRefCounted (base))
	    {
	      base->aliaskind = AK_NEWREF;
	      	    }
	}

          }
}

/*
** note: this side-effects sRefSet to set modified to TRUE
** for any sRef similar to s.
*/

bool
sRef_canModifyVal (sRef s, sRefSet sl)
{
  if (context_getFlag (FLG_MUSTMOD))
    {
      return (sRef_doModifyVal (s, sl));
    }
  else
    {
      return (sRef_checkModifyVal (s, sl));
    }
}

bool
sRef_canModify (sRef s, sRefSet sl)
{
  
  if (context_getFlag (FLG_MUSTMOD))
    {
      return (sRef_doModify (s, sl));
    }
  else
    {
      return (sRef_checkModify (s, sl));
    }
}

/*
** No side-effects
*/

static
bool sRef_checkModifyVal (sRef s, sRefSet sl)
{
  if (sRef_isInvalid (s))
    {
      return TRUE;
    }
  
  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CONST:
      return TRUE;
    case SK_CVAR:
      if (sRef_isGlobal (s))
	{
	  if (context_checkGlobMod (s))
	    {
	      return (sRefSet_member (sl, s));
	    }

	  return TRUE;
	}
      else
	{
	  return TRUE;
	}
    case SK_PARAM:
            return (sRefSet_member (sl, s) 
	      || alkind_isOnly (sRef_getOrigAliasKind (s)));
    case SK_ARRAYFETCH: 
      /* special case if ind known */
      return (sRefSet_member (sl, s) ||
	      sRef_checkModifyVal (s->info->arrayfetch->arr, sl));
    case SK_FIELD:
      return (sRefSet_member (sl, s) || sRef_checkModifyVal (s->info->field->rec, sl));
    case SK_PTR:
      return (sRefSet_member (sl, s) || sRef_checkModifyVal (s->info->ref, sl));
    case SK_ADR:
      return (sRefSet_member (sl, s) || sRef_checkModifyVal (s->info->ref, sl));
    case SK_CONJ:
      return ((sRef_checkModifyVal (s->info->conj->a, sl)) &&
	      (sRef_checkModifyVal (s->info->conj->b, sl)));
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_NEW:
    case SK_TYPE:
    case SK_DERIVED:
      return TRUE;
    case SK_EXTERNAL:
      return TRUE;
    case SK_SPECIAL:
      {
	switch (s->info->spec)
	  {
	  case SR_NOTHING:   return TRUE;
	  case SR_INTERNAL:  
	    if (context_getFlag (FLG_INTERNALGLOBS))
	      {
		return (sRefSet_member (sl, s));
	      }
	    else
	      {
		return TRUE;
	      }
	  case SR_SPECSTATE: return TRUE;
	  case SR_SYSTEM:    return (sRefSet_member (sl, s));
	  }
      }
    case SK_RESULT: BADBRANCH;
    }
  BADEXIT;
}

/*
** this should probably be elsewhere...
**
** returns TRUE iff sl indicates that s can be modified
*/

static bool sRef_checkModify (sRef s, sRefSet sl)
{
  llassert (sRef_isValid (s));

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CONST:
      return TRUE;
    case SK_CVAR:
      if (sRef_isGlobal (s))
	{
	  if (context_checkGlobMod (s))
	    {
	      return (sRefSet_member (sl, s));
	    }

	  return TRUE;
	}
      else
	{
	  return TRUE;
	}
    case SK_PARAM:
      return TRUE;
    case SK_ARRAYFETCH:
      return (sRefSet_member (sl, s) ||
	      sRef_checkModifyVal (s->info->arrayfetch->arr, sl));
    case SK_FIELD:
      {
	sRef sr = s->info->field->rec;

	if (sr->kind == SK_PARAM)
	  return TRUE; /* structs are copied on call */

	return (sRefSet_member (sl, s) || sRef_checkModifyVal (s->info->field->rec, sl));
      }
    case SK_PTR:
      {
	bool sm;

	sm = sRefSet_member (sl, s);

	if (sm)
	  return TRUE;
	else
	  return (sRef_checkModifyVal (s->info->ref, sl));
      }
    case SK_ADR:
      return (sRefSet_member (sl, s) || sRef_checkModifyVal (s->info->ref, sl));
    case SK_CONJ:
      return ((sRef_checkModify (s->info->conj->a, sl)) &&
	      (sRef_checkModify (s->info->conj->b, sl)));
    case SK_NEW:
    case SK_OBJECT:
    case SK_UNKNOWN:
    case SK_TYPE:
    case SK_DERIVED:
    case SK_EXTERNAL:
      return TRUE;
    case SK_SPECIAL:
      {
	switch (s->info->spec)
	  {
	  case SR_NOTHING:   return TRUE;
	  case SR_INTERNAL:  
	    if (context_getFlag (FLG_INTERNALGLOBS))
	      {
		return (sRefSet_member (sl, s));
	      }
	    else
	      {
		return TRUE;
	      }
	  case SR_SPECSTATE: return TRUE;
	  case SR_SYSTEM:    return (sRefSet_member (sl, s));
	  }
      }
    case SK_RESULT: BADBRANCH;
    }
  BADEXIT;
}

cstring sRef_stateVerb (sRef s)
{
  if (sRef_isDead (s))
    {
      return cstring_makeLiteralTemp ("released");
    }
  else if (sRef_isKept (s))
    {
      return cstring_makeLiteralTemp ("kept");
    }
  else if (sRef_isDependent (s))
    {
      return cstring_makeLiteralTemp ("dependent");
    }
  else
    {
      BADEXIT;
    }
}

cstring sRef_stateAltVerb (sRef s)
{
  if (sRef_isDead (s))
    {
      return cstring_makeLiteralTemp ("live");
    }
  else if (sRef_isKept (s))
    {
      return cstring_makeLiteralTemp ("not kept");
    }
  else if (sRef_isDependent (s))
    {
      return cstring_makeLiteralTemp ("independent");
    }
  else
    {
      BADEXIT;
    }
}

static 
bool sRef_doModifyVal (sRef s, sRefSet sl)
{
  llassert (sRef_isValid (s));

  
  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CONST:
      return TRUE;
    case SK_CVAR:
      if (sRef_isGlobal (s))
	{
	  
	  if (context_checkGlobMod (s))
	    {
	      return (sRefSet_modifyMember (sl, s));
	    }
	  else
	    {
	      (void) sRefSet_modifyMember (sl, s);
	    }

	  	  return TRUE;
	}
      else
	{
	  return TRUE;
	}      
    case SK_PARAM:
      return (sRefSet_modifyMember (sl, s) 
	      || alkind_isOnly (sRef_getOrigAliasKind (s)));
    case SK_ARRAYFETCH:
      /* special case if ind known */
      /* unconditional OR, need side effect */
      return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->arrayfetch->arr, sl)));
    case SK_FIELD:
      return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->field->rec, sl)));
    case SK_PTR:
      return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->ref, sl)));
    case SK_ADR:
      return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->ref, sl)));
    case SK_CONJ:
      return (AND (sRef_doModifyVal (s->info->conj->a, sl) ,
		   sRef_doModifyVal (s->info->conj->b, sl)));
    case SK_OBJECT:
    case SK_DERIVED:
    case SK_EXTERNAL:
    case SK_UNKNOWN:
    case SK_NEW:
    case SK_TYPE:
      return TRUE;
    case SK_SPECIAL:
      {
	switch (s->info->spec)
	  {
	  case SR_NOTHING:   return TRUE;
	  case SR_INTERNAL:  
	    if (context_getFlag (FLG_INTERNALGLOBS))
	      {
		return (sRefSet_modifyMember (sl, s));
	      }
	    else
	      {
		(void) sRefSet_modifyMember (sl, s);
		return TRUE;
	      }
	  case SR_SPECSTATE: 
	    {
	      return TRUE;
	    }
	  case SR_SYSTEM:
	    {
	      return (sRefSet_modifyMember (sl, s));
	    }
	  }
      }
    case SK_RESULT: BADBRANCH;
    }
  BADEXIT;
}

/*
** this should probably be elsewhere...
**
** returns TRUE iff sl indicates that s can be modified
*/

static 
bool sRef_doModify (sRef s, sRefSet sl)
{
    llassert (sRef_isValid (s));
  
  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CONST:
      return TRUE;
    case SK_CVAR:
      if (sRef_isGlobal (s))
	{
	  if (context_checkGlobMod (s))
	    {
	      return (sRefSet_modifyMember (sl, s));
	    }
	  else
	    {
	      (void) sRefSet_modifyMember (sl, s);
	    }

	  return TRUE;
	}
      else
	{
	  return TRUE;
	}
    case SK_PARAM:
      return TRUE;
    case SK_ARRAYFETCH:
            return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->arrayfetch->arr, sl)));
    case SK_FIELD:
      {
	sRef sr = s->info->field->rec;

	if (sr->kind == SK_PARAM)
	  {
	    return TRUE; /* structs are shallow-copied on call */
	  }
	
	return (OR (sRefSet_modifyMember (sl, s),
		    sRef_doModifyVal (s->info->field->rec, sl)));
      }
    case SK_PTR:
      {
	return (OR (sRefSet_modifyMember (sl, s),
		    sRef_doModifyVal (s->info->ref, sl)));
      }
    case SK_ADR:
      return (OR (sRefSet_modifyMember (sl, s),
		  sRef_doModifyVal (s->info->ref, sl)));
    case SK_CONJ:
      return (AND (sRef_doModify (s->info->conj->a, sl),
		  (sRef_doModify (s->info->conj->b, sl))));
    case SK_UNKNOWN:
    case SK_NEW:
    case SK_TYPE:
      return TRUE;
    case SK_OBJECT:
    case SK_DERIVED:
    case SK_EXTERNAL:
      return TRUE;
    case SK_SPECIAL:
      {
	switch (s->info->spec)
	  {
	  case SR_NOTHING:   return TRUE;
	  case SR_INTERNAL:  return TRUE;
	  case SR_SPECSTATE: return TRUE;
	  case SR_SYSTEM:    return (sRefSet_modifyMember (sl, s));
	  }
      }
    case SK_RESULT: BADBRANCH;
    }
  BADEXIT;
}

static /*@exposed@*/ sRef
  sRef_leastCommon (/*@exposed@*/ sRef s1, sRef s2)
{
  llassert (sRef_similar (s1, s2));
  
  if (!sRef_isValid (s1)) return s1;
  if (!sRef_isValid (s2)) return s1;

  sRef_combineDefState (s1, s2);
  sRef_combineNullState (s1, s2);
  sRef_combineExKinds (s1, s2);
  
  if (s1->aliaskind != s2->aliaskind)
    {
      if (s1->aliaskind == AK_UNKNOWN)
	{
	  s1->aliaskind = s2->aliaskind;
	}
      else if (s2->aliaskind == AK_UNKNOWN)
	{
	  ;
	}
      else
	{
	  s1->aliaskind = AK_ERROR;
	}
    }

  return s1;
}

int sRef_compare (sRef s1, sRef s2)
{
  if (s1 == s2) return 0;

  if (sRef_isInvalid (s1)) return -1;
  if (sRef_isInvalid (s2)) return 1;
      
  INTCOMPARERETURN (s1->kind, s2->kind);
  INTCOMPARERETURN (s1->defstate, s2->defstate);
  INTCOMPARERETURN (s1->aliaskind, s2->aliaskind);

  COMPARERETURN (nstate_compare (s1->nullstate, s2->nullstate));

  switch (s1->kind)
    {
    case SK_PARAM:
      return (int_compare (s1->info->paramno, s2->info->paramno));
    case SK_ARRAYFETCH:
      {
	COMPARERETURN (sRef_compare (s1->info->arrayfetch->arr, 
				     s2->info->arrayfetch->arr));
	
	if (s1->info->arrayfetch->indknown && s2->info->arrayfetch->indknown)
	  {
	    return (int_compare (s1->info->arrayfetch->ind, 
				 s2->info->arrayfetch->ind));
	  }
	if (!s1->info->arrayfetch->indknown && !s2->info->arrayfetch->indknown)
	  return 0;
	
	return 1;
      }
    case SK_FIELD:
      {
	COMPARERETURN (sRef_compare (s1->info->field->rec, s2->info->field->rec));
	
	if (cstring_equal (s1->info->field->field, s2->info->field->field))
	  return 0;

	return 1;
      }
    case SK_PTR:
    case SK_ADR:
      return (sRef_compare (s1->info->ref, s2->info->ref));
    case SK_CONJ:
      COMPARERETURN (sRef_compare (s1->info->conj->a, s2->info->conj->a));
      return (sRef_compare (s1->info->conj->b, s2->info->conj->b));
    case SK_UNCONSTRAINED:
      return (cstring_compare (s1->info->fname, s2->info->fname));
    case SK_NEW:
    case SK_CVAR:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_TYPE:
    case SK_DERIVED:
    case SK_EXTERNAL:
    case SK_CONST:
    case SK_RESULT:
      return 0;
    case SK_SPECIAL:
      return (generic_compare (s1->info->spec, s2->info->spec));
    }
  BADEXIT;
}

static bool cref_equal (cref c1, cref c2)
{
  return ((c1->lexlevel == c2->lexlevel) &&
	  (usymId_equal (c1->index, c2->index)));
}

/*
** returns true if s1 could be the same storage as s2.
** i.e., a[?] ~ a[3].  Note its not symmetric ... s1
** should be more specific.
*/

/*
** like similar, but matches objects <-> non-objects
*/

static bool 
sRef_uniqueReference (sRef s)
{
  return (sRef_isFresh (s) || sRef_isUnique (s) 
	  || sRef_isOnly (s) || sRef_isStack (s)
	  || sRef_isAddress (s)); 
}

static bool
sRef_similarRelaxedAux (sRef s1, sRef s2)
{
  if (s1 == s2)
    {
      if (sRef_isUnknownArrayFetch (s1))
	{
	  return FALSE;
	}
      else
	{
	  return TRUE;
	}
    }

  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  if (sRef_isConj (s2)) 
    return (sRef_similarRelaxedAux (s1, sRef_getConjA (s2)) ||
	    sRef_similarRelaxedAux (s1, sRef_getConjB (s2)));

  switch (s1->kind)
    {
    case SK_CVAR:
      return ((s2->kind == SK_CVAR)
	      && (cref_equal (s1->info->cvar, s2->info->cvar)));
    case SK_PARAM:
      return ((s2->kind == SK_PARAM)
	      && (s1->info->paramno == s2->info->paramno));
    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  if (sRef_similarRelaxedAux (s1->info->arrayfetch->arr,
				      s2->info->arrayfetch->arr))
	    {
	      if (s1->info->arrayfetch->indknown)
		{
		  if (s2->info->arrayfetch->indknown)
		    {
		      return (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind);
		    }
		  else 
		    {
		      return FALSE;
		    }
		}
	      else
		{
		  return FALSE;
		}
	    }
	}
      return FALSE;
    case SK_FIELD:
      return ((s2->kind == SK_FIELD
	       && (sRef_similarRelaxedAux (s1->info->field->rec,
					   s2->info->field->rec)
		   && cstring_equal (s1->info->field->field,
				     s2->info->field->field))));
    case SK_PTR:
      return ((s2->kind == SK_PTR)
	      && sRef_similarRelaxedAux (s1->info->ref, s2->info->ref));
    case SK_ADR:
      return ((s2->kind == SK_ADR)
	      && sRef_similarRelaxedAux (s1->info->ref, s2->info->ref));
    case SK_CONJ:
      return ((sRef_similarRelaxedAux (s1->info->conj->a, s2) ||
	      (sRef_similarRelaxedAux (s1->info->conj->b, s2))));
    case SK_SPECIAL:
      return (s1->info->spec == s2->info->spec);
    case SK_UNCONSTRAINED:
      return (cstring_equal (s1->info->fname, s2->info->fname));
    case SK_DERIVED:
    case SK_CONST:
    case SK_TYPE:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:
      return FALSE;
    }
  BADEXIT;
}

bool
sRef_similarRelaxed (sRef s1, sRef s2)
{
  bool us1, us2;

  if (s1 == s2) 
    {
      if (sRef_isThroughArrayFetch (s1))
	{
	  return FALSE;
	}
      else
	{
	  return TRUE;
	}
    }

  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  us1 = sRef_uniqueReference (s1);
  us2 = sRef_uniqueReference (s2);

  if ((s1->kind == SK_EXTERNAL && (s2->kind != SK_EXTERNAL && !us2))
      || (s2->kind == SK_EXTERNAL && (s1->kind != SK_EXTERNAL && !us1)))
    {
      /*
      ** Previously, also:
      **   || (sRef_isExposed (s1) && !us2) || (sRef_isExposed (s2) && !us1)) ???? 
      **
      ** No clue why this was there?!
      */


      if (sRef_isExposed (s1) && sRef_isCvar (s1))
	{
	  uentry ue1 = sRef_getUentry (s1);

	  if (uentry_isRefParam (ue1))
	    {
	      return sRef_similarRelaxedAux (s1, s2);
	    }
	}
      
      if (sRef_isExposed (s2) && sRef_isCvar (s2))
	{
	  uentry ue2 = sRef_getUentry (s2);

	  if (uentry_isRefParam (ue2))
	    {
	      return sRef_similarRelaxedAux (s1, s2);
	    }
	}
      
            return (ctype_match (s1->type, s2->type));
    }
  else
    {
            return sRef_similarRelaxedAux (s1, s2);
    }
}

bool
sRef_similar (sRef s1, sRef s2)
{
  if (s1 == s2) return TRUE;
  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  if (sRef_isConj (s2)) 
    {
      return (sRef_similar (s1, sRef_getConjA (s2)) ||
	      sRef_similar (s1, sRef_getConjB (s2)));
    }

  if (sRef_isDerived (s2))
   {
     return (sRef_includedBy (s1, s2->info->ref));
   }

  switch (s1->kind)
    {
    case SK_CVAR:
      return ((s2->kind == SK_CVAR)
	      && (cref_equal (s1->info->cvar, s2->info->cvar)));
    case SK_PARAM:
      return ((s2->kind == SK_PARAM)
	      && (s1->info->paramno == s2->info->paramno));
    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  if (sRef_similar (s1->info->arrayfetch->arr,
			    s2->info->arrayfetch->arr))
	    {
	      if (s1->info->arrayfetch->indknown)
		{
		  if (s2->info->arrayfetch->indknown)
		    {
		      return (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind);
		    }
		  else 
		    {
		      return TRUE;
		    }
		}
	      else
		{
		  return TRUE;
		}
	    }
	}
      else 
	{
	  if (s2->kind == SK_PTR)
	    {
	      if (sRef_similar (s1->info->arrayfetch->arr,
				s2->info->ref))
		{
		  return TRUE; 
		}
	    }
	}

      return FALSE;
    case SK_FIELD:
      return ((s2->kind == SK_FIELD
	       && (sRef_similar (s1->info->field->rec,
				 s2->info->field->rec)
		   && cstring_equal (s1->info->field->field,
				     s2->info->field->field))));
    case SK_PTR:
      if (s2->kind == SK_PTR)
	{
	  return sRef_similar (s1->info->ref, s2->info->ref);
	}
      else 
	{
	  if (s2->kind == SK_ARRAYFETCH)
	    {
	      if (sRef_similar (s2->info->arrayfetch->arr,
				s1->info->ref))
		{
		  return TRUE; 
		}
	    }
	}

      return FALSE;
    case SK_ADR:
      return ((s2->kind == SK_ADR)
	      && sRef_similar (s1->info->ref, s2->info->ref));
    case SK_CONJ:
      return ((sRef_similar (s1->info->conj->a, s2) ||
	      (sRef_similar (s1->info->conj->b, s2))));
    case SK_DERIVED:
      return (sRef_includedBy (s2, s1->info->ref));
    case SK_UNCONSTRAINED:
      return (s2->kind == SK_UNCONSTRAINED
	      && cstring_equal (s1->info->fname, s2->info->fname));
    case SK_CONST:
    case SK_TYPE:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:
      return FALSE;
    case SK_SPECIAL:
      return (s2->kind == SK_SPECIAL 
	      && (s1->info->spec == s2->info->spec));
    }

  /*@notreached@*/ DPRINTF (("Fell through for: %s / %s", sRef_unparse (s1), sRef_unparse (s2)));
  BADEXIT;
}

/*
** return TRUE iff small can be derived from big.
**
** (e.g. x, x.a is includedBy x;
**       x.a is included By x.a;
*/

bool
sRef_includedBy (sRef small, sRef big)
{
  if (small == big) return TRUE;
  if (sRef_isInvalid (small) || sRef_isInvalid (big)) return FALSE;

  if (sRef_isConj (big)) 
    return (sRef_similar (small, sRef_getConjA (big)) ||
	    sRef_similar (small, sRef_getConjB (big)));

  switch (small->kind)
    {
    case SK_CVAR:
    case SK_PARAM:
      return (sRef_same (small, big));
    case SK_ARRAYFETCH:
      if (big->kind == SK_ARRAYFETCH)
	{
	  if (sRef_same (small->info->arrayfetch->arr, big->info->arrayfetch->arr))
	    {
	      if (small->info->arrayfetch->indknown)
		{
		  if (big->info->arrayfetch->indknown)
		    {
		      return (small->info->arrayfetch->ind == big->info->arrayfetch->ind);
		    }
		  else 
		    {
		      return TRUE;
		    }
		}
	      else
		{
		  return TRUE;
		}
	    }
	}
      return (sRef_includedBy (small->info->arrayfetch->arr, big));
    case SK_FIELD:
      if (big->kind == SK_FIELD)
	{
	  return 
	    (sRef_same (small->info->field->rec, big->info->field->rec) &&
	     cstring_equal (small->info->field->field, big->info->field->field));
	}
      else
	{
	  return (sRef_includedBy (small->info->field->rec, big));
	}

    case SK_PTR:
      if (big->kind == SK_PTR)
	{
	  return sRef_same (small->info->ref, big->info->ref);
	}
      else
	{
	  return (sRef_includedBy (small->info->ref, big));
	}

    case SK_ADR:
      return ((big->kind == SK_ADR) && sRef_similar (small->info->ref, big->info->ref));
    case SK_CONJ:
      return ((sRef_includedBy (small->info->conj->a, big) ||
	      (sRef_includedBy (small->info->conj->b, big))));
    case SK_DERIVED:
      return (sRef_includedBy (small->info->ref, big));
    case SK_UNCONSTRAINED:
    case SK_CONST:
    case SK_TYPE:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:
      return FALSE;
    case SK_SPECIAL:
      switch (small->info->spec)
	{
	case SR_NOTHING: return TRUE;
	case SR_SPECSTATE:
	case SR_INTERNAL: return (sRef_isSpecInternalState (big) ||
				  sRef_isFileStatic (big));
	case SR_SYSTEM: return (sRef_isSystemState (big));
	}
    }
  BADEXIT;
}

/*
** Same is similar to similar, but not quite the same. 
** same and realSame aren't the same, but they are really similar.
** similarly, same is the same as same. but realSame is
** not really the same as same, or similar to similar.
**
** Similarly to similar, same checks if two sRefs are the same.
** The similarities end, however, when same compares arrays
** with unknown indexes.  Similar returns false; same returns true.
**
** Similarly to similar and same, realSame is the same as same,
** except they do not behave the same when face with unknown
** sRefs.  Same thinks they are not the same, but realSame thinks
** the are.
**
*/

bool
sRef_realSame (sRef s1, sRef s2)
{
  if (s1 == s2) return TRUE;  
  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  switch (s1->kind)
    {
    case SK_CVAR:
      return ((s2->kind == SK_CVAR) && (cref_equal (s1->info->cvar, s2->info->cvar)));
    case SK_PARAM:
      return ((s2->kind == SK_PARAM) && (s1->info->paramno == s2->info->paramno));
    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  if (sRef_realSame (s1->info->arrayfetch->arr, s2->info->arrayfetch->arr))
	    {
	      if (s1->info->arrayfetch->indknown && s2->info->arrayfetch->indknown)
		{
		  return (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind);
		}
	      if (!s1->info->arrayfetch->indknown && !s2->info->arrayfetch->indknown)
		{
		  return TRUE;
		}
	      return FALSE;
	    }
	}
      return FALSE;
    case SK_FIELD:
      return ((s2->kind == SK_FIELD &&
	       (sRef_realSame (s1->info->field->rec, s2->info->field->rec) &&
		cstring_equal (s1->info->field->field, s2->info->field->field))));
    case SK_PTR:
      return ((s2->kind == SK_PTR) && sRef_realSame (s1->info->ref, s2->info->ref));
    case SK_ADR:
      return ((s2->kind == SK_ADR) && sRef_realSame (s1->info->ref, s2->info->ref));
    case SK_CONJ:
      return ((sRef_realSame (s1->info->conj->a, s2) ||
	      (sRef_realSame (s1->info->conj->b, s2))));
    case SK_OBJECT:
      return ((s2->kind == SK_OBJECT) 
	      && ctype_match (s1->info->object, s2->info->object));
    case SK_EXTERNAL:
      return ((s2->kind == SK_EXTERNAL) 
	      && sRef_realSame (s1->info->ref, s2->info->ref));
    case SK_SPECIAL:
      return ((s2->kind == SK_SPECIAL) && s1->info->spec == s2->info->spec);
    case SK_DERIVED:
      return ((s2->kind == SK_DERIVED) && sRef_realSame (s1->info->ref, s2->info->ref));
    case SK_UNCONSTRAINED:
      return ((s2->kind == SK_UNCONSTRAINED) 
	      && (cstring_equal (s1->info->fname, s2->info->fname)));
    case SK_TYPE:
    case SK_CONST:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_RESULT:
      return TRUE; /* changed this! was false */
    }
  BADEXIT;
}

/*
** same is similar to similar, but not quite the same. 
**
** Similarly to similar, same checks is two sRefs are the same.
** The similarities end, however, when same compares arrays
** with unknown indexes.  Similar returns false; same returns true.
*/

bool
sRef_same (sRef s1, sRef s2)
{
  if (s1 == s2) return TRUE;
  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  switch (s1->kind)
    {
    case SK_CVAR:
      return ((s2->kind == SK_CVAR) && (cref_equal (s1->info->cvar, s2->info->cvar)));
    case SK_PARAM:
      return ((s2->kind == SK_PARAM) && (s1->info->paramno == s2->info->paramno));
    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  llassert (s1->info->field->rec != s1);
	  if (sRef_same (s1->info->arrayfetch->arr, s2->info->arrayfetch->arr))
	    {
	      if (s1->info->arrayfetch->indknown && s2->info->arrayfetch->indknown)
		{
		  return (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind);
		}
	      return TRUE;
	    }
	}
      return FALSE;
    case SK_FIELD:
      {
	llassert (s1->info->field->rec != s1);
	return ((s2->kind == SK_FIELD &&
		 (sRef_same (s1->info->field->rec, s2->info->field->rec) &&
		  cstring_equal (s1->info->field->field, s2->info->field->field))));
      }
    case SK_PTR:
      {
	llassert (s1->info->ref != s1);
	return ((s2->kind == SK_PTR) && sRef_same (s1->info->ref, s2->info->ref));
      }
    case SK_ADR:
      {
	llassert (s1->info->ref != s1);
	return ((s2->kind == SK_ADR) && sRef_same (s1->info->ref, s2->info->ref));
      }
    case SK_CONJ:
      llassert (s1->info->conj->a != s1);
      llassert (s1->info->conj->b != s1);
      return ((sRef_same (s1->info->conj->a, s2)) && /* or or and? */
	      (sRef_same (s1->info->conj->b, s2)));
    case SK_SPECIAL:
      return ((s2->kind == SK_SPECIAL) && s1->info->spec == s2->info->spec);
    case SK_DERIVED:
      llassert (s1->info->ref != s1);
      return ((s2->kind == SK_DERIVED) && sRef_same (s1->info->ref, s2->info->ref));
    case SK_CONST:
    case SK_UNCONSTRAINED:
    case SK_TYPE:
    case SK_UNKNOWN:
    case SK_NEW:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:
      return FALSE; 
    }
  BADEXIT;
}

/*
** sort of similar, for use in def/use
*/

static bool
sRef_closeEnough (sRef s1, sRef s2)
{
  if (s1 == s2) return TRUE;
  if (sRef_isInvalid (s1) || sRef_isInvalid (s2)) return FALSE;

  switch (s1->kind)
    {
    case SK_CVAR:
      return (((s2->kind == SK_CVAR) &&
	       (cref_equal (s1->info->cvar, s2->info->cvar))) ||
	      (s2->kind == SK_UNCONSTRAINED && s1->info->cvar->lexlevel == 0));
    case SK_UNCONSTRAINED:
      return (s2->kind == SK_UNCONSTRAINED
	      || ((s2->kind == SK_CVAR) && (s2->info->cvar->lexlevel == 0)));
    case SK_PARAM:
      return ((s2->kind == SK_PARAM) 
	      && (s1->info->paramno == s2->info->paramno));
    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  if (sRef_closeEnough (s1->info->arrayfetch->arr, s2->info->arrayfetch->arr))
	    {
	      if (s1->info->arrayfetch->indknown && s2->info->arrayfetch->indknown)
		{
		  return (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind);
		}
	      return TRUE;
	    }
	}
      return FALSE;
    case SK_FIELD:
      return ((s2->kind == SK_FIELD &&
	       (sRef_closeEnough (s1->info->field->rec, s2->info->field->rec) &&
		cstring_equal (s1->info->field->field, s2->info->field->field))));
    case SK_PTR:
      return ((s2->kind == SK_PTR) && sRef_closeEnough (s1->info->ref, s2->info->ref));
    case SK_ADR:
      return ((s2->kind == SK_ADR) && sRef_closeEnough (s1->info->ref, s2->info->ref));
    case SK_DERIVED:
      return ((s2->kind == SK_DERIVED) && sRef_closeEnough (s1->info->ref, s2->info->ref));
    case SK_CONJ:
      return ((sRef_closeEnough (s1->info->conj->a, s2)) ||
	      (sRef_closeEnough (s1->info->conj->b, s2)));
    case SK_SPECIAL:
      return ((s2->kind == SK_SPECIAL) && s1->info->spec == s2->info->spec);
    case SK_TYPE:
    case SK_CONST:
    case SK_UNKNOWN:
    case SK_NEW:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:

      return FALSE;
    }
  BADEXIT;
}

/*
  drl add 12/24/2000
  s is an sRef of a formal paramenter in a function call constraint
  we trys to return a constraint expression derived from the actual parementer of a function call.
*/
constraintExpr sRef_fixConstraintParam ( sRef s, exprNodeList args)
{
  constraintExpr ce;

  if (sRef_isInvalid (s))
    llfatalbug(("Invalid sRef"));

  if (s->kind == SK_RESULT)
    {
      ce = constraintExpr_makeTermsRef (s);
      return ce;
    }
  if (s->kind != SK_PARAM)
    {
      if (s->kind != SK_CVAR)
	{
	  llcontbug ((message("Trying to do fixConstraintParam on nonparam, nonglobal: %s for function with arguments %s", sRef_unparse (s), exprNodeList_unparse(args) ) ));
	}
      ce = constraintExpr_makeTermsRef (s);
      return ce;
    }
  
  if (exprNodeList_size (args) > s->info->paramno)
    {
      exprNode e = exprNodeList_nth (args, s->info->paramno);
      
      if (exprNode_isError (e))
	{
	  llassert (FALSE);
	}
      
      ce = constraintExpr_makeExprNode (e);
    }
  else
    {
      llassert(FALSE);
    }
  return ce;
}

/*@exposed@*/ sRef
sRef_fixBaseParam (/*@returned@*/ sRef s, exprNodeList args)
{
  if (sRef_isInvalid (s)) return (sRef_undefined);

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CVAR:
      return s;
    case SK_PARAM:
      {
	if (exprNodeList_size (args) > s->info->paramno)
	  {
	    exprNode e = exprNodeList_nth (args, s->info->paramno);

	    if (exprNode_isError (e))
	      {
		return sRef_makeUnknown ();
	      }
	    
	    return (exprNode_getSref (e));
	  }
	else
	  {
	    return sRef_makeUnknown ();
	  }
      }
    case SK_ARRAYFETCH:

      if (s->info->arrayfetch->indknown)
	{
	  return (sRef_makeArrayFetchKnown 
		  (sRef_fixBaseParam (s->info->arrayfetch->arr, args),
		   s->info->arrayfetch->ind));
	}
      else
	{
	  return (sRef_makeArrayFetch 
		  (sRef_fixBaseParam (s->info->arrayfetch->arr, args)));
	}
    case SK_FIELD:
      return (sRef_makeField (sRef_fixBaseParam (s->info->field->rec, args),
			      s->info->field->field));

    case SK_PTR:
      return (sRef_makePointer (sRef_fixBaseParam (s->info->ref, args)));

    case SK_ADR:
      return (sRef_makeAddress (sRef_fixBaseParam (s->info->ref, args)));

    case SK_CONJ:
      return (sRef_makeConj (sRef_fixBaseParam (s->info->conj->a, args),
			     sRef_fixBaseParam (s->info->conj->b, args)));
    case SK_DERIVED:
    case SK_SPECIAL:
    case SK_TYPE:
    case SK_CONST:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_EXTERNAL:
    case SK_RESULT:
      return s;
    }
  BADEXIT;
}

/*@exposed@*/ sRef
sRef_undumpGlobal (char **c)
{
  char p = **c;

  (*c)++;

  switch (p)
    {
    case 'g':
      {
	usymId uid = usymId_fromInt (getInt (c));
	sstate defstate;
	nstate nullstate;
	sRef ret;

	checkChar (c, '@');
	defstate = sstate_fromInt (getInt (c));

	checkChar (c, '@');
	nullstate = nstate_fromInt (getInt (c));

	ret = sRef_makeGlobal (uid, ctype_unknown);
	ret->nullstate = nullstate;
	ret->defstate = defstate;
	return ret;
      }
    case 's':
      {
	int i = getInt (c);
	speckind sk = speckind_fromInt (i);

	switch (sk)
	  {
	  case SR_NOTHING:   return (sRef_makeNothing ());
	  case SR_INTERNAL:  return (sRef_makeInternalState ());
	  case SR_SPECSTATE: return (sRef_makeSpecState ());
	  case SR_SYSTEM:    return (sRef_makeSystemState ());
	  }
	BADEXIT;
      }
    case '-':
      return sRef_undefined;
    case 'u':
      return sRef_makeUnknown ();
    case 'x':
      return sRef_makeUnknown ();
    default:
      llfatalerror (message ("sRef_undumpGlobal: bad line: %s",
			     cstring_fromChars (*c)));
    }
  BADEXIT;
}

/*@exposed@*/ sRef
sRef_undump (char **c)
{
  char p = **c;

  (*c)++;

  switch (p)
    {
    case 'g':
      return (sRef_makeGlobal (usymId_fromInt (getInt (c)), ctype_unknown));
    case 'p':
      return (sRef_makeParam (getInt (c), ctype_unknown));
    case 'r':
      return (sRef_makeResultType (ctype_undump (c)));
    case 'a':
      {
	if ((**c >= '0' && **c <= '9') || **c == '-')
	  {
	    int i = getInt (c);
	    sRef arr = sRef_undump (c);
	    sRef ret = sRef_buildArrayFetchKnown (arr, i);

	    return ret;
	  }
	else
	  {
	    sRef arr = sRef_undump (c);
	    sRef ret = sRef_buildArrayFetch (arr);

	    return ret;
	  }
      }
    case 'f':
      {
	cstring fname = cstring_undefined;
	sRef ret;

	while (**c != '.')
	  {
	    fname = cstring_appendChar (fname, **c);
	    (*c)++;
	  }
	(*c)++;

	ret = sRef_buildField (sRef_undump (c), fname);
	cstring_markOwned (fname);
	return (ret);
      }
    case 's':
      {
	int i = getInt (c);
	speckind sk = speckind_fromInt (i);

	switch (sk)
	  {
	  case SR_NOTHING:   return (sRef_makeNothing ());
	  case SR_INTERNAL:  return (sRef_makeInternalState ());
	  case SR_SPECSTATE: return (sRef_makeSpecState ());
	  case SR_SYSTEM:    return (sRef_makeSystemState ());
	  }
	BADEXIT;
      }
    case 't':
      {
	sRef ptr = sRef_undump (c);
	sRef ret = sRef_makePointer (ptr);

	return (ret);
      }
    case 'd':
      {
	sRef adr = sRef_undump (c);
	sRef ret = sRef_makeAddress (adr);

	return (ret);
      }
    case 'o':
      {
	return (sRef_makeObject (ctype_undump (c)));
      }
    case 'c':
      {
	sRef s1 = sRef_undump (c);
	sRef s2 = ((*c)++, sRef_undump (c));
	sRef ret = sRef_makeConj (s1, s2);

	return (ret);
      }
    case '-':
      return sRef_undefined;
    case 'u':
      return sRef_makeUnknown ();
    case 'x':
      return sRef_makeUnknown ();
    default:
      llfatalerror (message ("sRef_undump: bad line: %s", cstring_fromChars (*c)));
    }
  BADEXIT;
}

/*@only@*/ cstring
sRef_dump (sRef s)
{
  if (sRef_isInvalid (s))
    {
      return (cstring_makeLiteral ("-"));
    }
  else
    {
      switch (s->kind)
	{
	case SK_PARAM:
	  return (message ("p%d", s->info->paramno));
	case SK_ARRAYFETCH:
	  if (s->info->arrayfetch->indknown)
	    {
	      return (message ("a%d%q", s->info->arrayfetch->ind,
			       sRef_dump (s->info->arrayfetch->arr)));
	    }
	  else
	    {
	      return (message ("a%q", sRef_dump (s->info->arrayfetch->arr)));
	    }
	case SK_FIELD:
	  return (message ("f%s.%q", s->info->field->field, 
			   sRef_dump (s->info->field->rec)));
	case SK_PTR:
	  return (message ("t%q", sRef_dump (s->info->ref)));
	case SK_ADR:
	  return (message ("d%q", sRef_dump (s->info->ref)));
	case SK_OBJECT:
	  return (message ("o%q", ctype_dump (s->info->object)));
	case SK_SPECIAL:
	  return (message ("s%d", (int) s->info->spec));
	case SK_CONJ:
	  return (message ("c%q.%q",
			   sRef_dump (s->info->conj->a),
			   sRef_dump (s->info->conj->b)));
	case SK_CVAR:
	  if (sRef_isGlobal (s))
	    {
	      return (message ("g%d", 
			       usymtab_convertId (s->info->cvar->index)));
	    }
	  else
	    {
	      llcontbug (message ("Dumping local variable: %q",
				  sRef_unparseDebug (s)));
	      return (cstring_makeLiteral ("u"));
	    }
	case SK_UNKNOWN:
	  return (cstring_makeLiteral ("u"));
	case SK_RESULT:
	  return (message ("r%q", ctype_dump (s->type)));
	case SK_TYPE:
	case SK_CONST:
	case SK_EXTERNAL:
	case SK_DERIVED:
	case SK_NEW:
	case SK_UNCONSTRAINED:
	  llcontbug (message ("sRef_dump: bad kind: %q",
			      sRef_unparseFull (s)));
	  return (cstring_makeLiteral ("x"));
	}
    }
     
  BADEXIT;
}

cstring sRef_dumpGlobal (sRef s)
{
  if (sRef_isInvalid (s))
    {
      return (cstring_makeLiteral ("-"));
    }
  else
    {
      switch (s->kind)
	{
	case SK_CVAR:
	  if (sRef_isGlobal (s))
	    {
	      return (message ("g%d@%d@%d", 
			       usymtab_convertId (s->info->cvar->index),
			       (int) s->defstate,
			       (int) s->nullstate));
	    }
	  else
	    {
	      llcontbug (message ("Dumping local variable: %q",
				  sRef_unparseDebug (s)));
	      return (cstring_makeLiteral ("u"));
	    }
	case SK_UNKNOWN:
	  return (cstring_makeLiteral ("u"));
	case SK_SPECIAL:
	  return (message ("s%d", (int) s->info->spec));
	default:
	  llcontbug (message ("sRef_dumpGlobal: bad kind: %q",
			      sRef_unparseFull (s)));
	  return (cstring_makeLiteral ("x"));
	}
    }
     
  BADEXIT;
}

ctype
sRef_deriveType (sRef s, uentryList cl)
{
  if (sRef_isInvalid (s)) return ctype_unknown;

  switch (s->kind)
    {
    case SK_CVAR:
      return (uentry_getType (usymtab_getRefQuiet (s->info->cvar->lexlevel, 
					      s->info->cvar->index)));
    case SK_UNCONSTRAINED:
      return (ctype_unknown);
    case SK_PARAM:
      return uentry_getType (uentryList_getN (cl, s->info->paramno));
    case SK_ARRAYFETCH:
      {
	ctype ca = sRef_deriveType (s->info->arrayfetch->arr, cl);
	
	if (ctype_isArray (ca))
	  {
	    return (ctype_baseArrayPtr (ca));
	  }
	else if (ctype_isUnknown (ca))
	  {
	    return (ca);
	  }
	else
	  {
	    llcontbuglit ("sRef_deriveType: inconsistent array type");
	    return ca;
	  }
      }
    case SK_FIELD:
      {
	ctype ct = sRef_deriveType (s->info->field->rec, cl);
	
	if (ctype_isStructorUnion (ct))
	  {
	    uentry ue = uentryList_lookupField (ctype_getFields (ct), 
					       s->info->field->field);
	    
	    if (uentry_isValid (ue))
	      {
		return (uentry_getType (ue));
	      }
	    else
	      {
		llcontbuglit ("sRef_deriveType: bad field");
		return ctype_unknown;
	      }
	  }
	else if (ctype_isUnknown (ct))
	  {
	    return (ct);
	  }
	else
	  {
	    llcontbuglit ("sRef_deriveType: inconsistent field type");
	    return (ct);
	  }
      }
    case SK_PTR:
      {
	ctype ct = sRef_deriveType (s->info->ref, cl);
	
	if (ctype_isUnknown (ct)) return ct;
	if (ctype_isPointer (ct)) return (ctype_baseArrayPtr (ct));
	else
	  {
	    llcontbuglit ("sRef_deriveType: inconsistent pointer type");
	    return (ct);
	  }
      }
    case SK_ADR:
      {
	ctype ct = sRef_deriveType (s->info->ref, cl);
	
	if (ctype_isUnknown (ct)) return ct;
	return ctype_makePointer (ct);
      }
    case SK_DERIVED:
      {
	return sRef_deriveType (s->info->ref, cl);
      }
    case SK_OBJECT:
      {
	return (s->info->object);
      }
    case SK_CONJ:
      {
	return (ctype_makeConj (sRef_deriveType (s->info->conj->a, cl),
			       sRef_deriveType (s->info->conj->b, cl)));
      }
    case SK_RESULT:
    case SK_CONST:
    case SK_TYPE:
      {
	return (s->type);
      }
    case SK_SPECIAL:
    case SK_UNKNOWN:
    case SK_EXTERNAL:
    case SK_NEW:
      return ctype_unknown;
    }
  BADEXIT;
}

ctype
sRef_getType (sRef s)
{
  if (sRef_isInvalid (s)) return ctype_unknown;
  return s->type;
}


/*@only@*/ cstring
sRef_unparseOpt (sRef s)
{
  sRef rb = sRef_getRootBase (s);

  if (sRef_isMeaningful (rb) && !sRef_isConst (rb))
    {
      cstring ret = sRef_unparse (s);
      
      llassertprint (!cstring_equalLit (ret, "?"), ("print: %s", sRef_unparseDebug (s)));

      if (!cstring_isEmpty (ret))
	{
	  return (cstring_appendChar (ret, ' '));
	}
      else
	{
	  return ret;
	}
    }

  return cstring_undefined;
}

cstring
sRef_unparsePreOpt (sRef s)
{
  sRef rb = sRef_getRootBase (s);

  if (sRef_isMeaningful (rb) && !sRef_isConst (rb))
    {
      cstring ret = sRef_unparse (s);
      
      llassertprint (!cstring_equalLit (ret, "?"), ("print: %s", sRef_unparseDebug (s)));
      return (cstring_prependCharO (' ', ret));
    }

  return cstring_undefined;
}

/*@only@*/ cstring
sRef_unparse (sRef s)
{
  if (sRef_isInvalid (s)) return (cstring_makeLiteral ("?"));

  if (context_inFunctionLike ())
    {
      return (sRef_unparseWithArgs (s, context_getParams ()));
    }
  else
    {
      return (sRef_unparseNoArgs (s));
    }
}

static /*@only@*/ cstring
sRef_unparseWithArgs (sRef s, uentryList args)
{
  if (sRef_isInvalid (s))
    {
      return (cstring_makeLiteral ("?"));
    }

  switch (s->kind)
    {
    case SK_CVAR:
      return (uentry_getName (usymtab_getRefQuiet (s->info->cvar->lexlevel,
						   s->info->cvar->index)));
    case SK_UNCONSTRAINED:
      return (cstring_copy (s->info->fname));
    case SK_PARAM:
      {
	if (s->info->paramno < uentryList_size (args))
	  {
	    uentry ue = uentryList_getN (args, s->info->paramno);
	    
	    if (uentry_isValid (ue))
	      return uentry_getName (ue);
	  }

	return (message ("<bad param: %q / args %q",
			 sRef_unparseDebug (s),
			 uentryList_unparse (args)));
      }
    case SK_ARRAYFETCH:
      if (s->info->arrayfetch->indknown)
	{
	  return (message ("%q[%d]", sRef_unparseWithArgs (s->info->arrayfetch->arr, args),
				s->info->arrayfetch->ind));
	}
      else
	{
	  return (message ("%q[]", sRef_unparseWithArgs (s->info->arrayfetch->arr, args)));
	}
    case SK_FIELD:
      if (s->info->field->rec->kind == SK_PTR)
	{
	  sRef ptr = s->info->field->rec;

	  return (message ("%q->%s", sRef_unparseWithArgs (ptr->info->ref, args),
			   s->info->field->field));	  
	}
      return (message ("%q.%s", sRef_unparseWithArgs (s->info->field->rec, args),
		       s->info->field->field));

    case SK_PTR:
      {
	sRef ref = sRef_fixConj (s->info->ref);
	skind sk = ref->kind;
	cstring ret;

	if (sk == SK_NEW)
	  {
	    ret = message ("storage pointed to by %q",
			   sRef_unparseWithArgs (ref, args));
	  }
	else if (skind_isSimple (sk) || sk == SK_PTR)
	  {
	    ret = message ("*%q", sRef_unparseWithArgs (ref, args));
	  }
	else
	  {
	    ret = message ("*(%q)", sRef_unparseWithArgs (ref, args));
	  }

	return ret;
      }
    case SK_ADR:
      return (message ("&%q", sRef_unparseWithArgs (s->info->ref, args)));
    case SK_OBJECT:
      return (cstring_copy (ctype_unparse (s->info->object)));
    case SK_CONJ:
      return (sRef_unparseWithArgs (sRef_getConjA (s), args));
    case SK_NEW:
      if (cstring_isDefined (s->info->fname))
	{
	  return (message ("[result of %s]", s->info->fname));
	}
      else
	{
	  return (cstring_makeLiteral ("<new>"));
	}
    case SK_UNKNOWN:
      return (cstring_makeLiteral ("?"));
    case SK_DERIVED:
      return (message ("<derived %q>", sRef_unparse (s->info->ref)));
    case SK_EXTERNAL:
      return (message ("<external %q>", sRef_unparse (s->info->ref)));
    case SK_TYPE:
      return (message ("<type %s>", ctype_unparse (s->type)));
    case SK_CONST:
      return (message ("<const %s>", ctype_unparse (s->type)));
    case SK_SPECIAL:
      return (cstring_makeLiteral
	      (s->info->spec == SR_NOTHING ? "nothing"
	       : s->info->spec == SR_INTERNAL ? "internal state"
	       : s->info->spec == SR_SPECSTATE ? "spec state"
	       : s->info->spec == SR_SYSTEM ? "file system state"
	       : "<spec error>"));
    case SK_RESULT:
      return cstring_makeLiteral ("result");
    default:
      {
	llbug (message ("Bad sref, kind = %d", (int) s->kind));
      }
    }

  BADEXIT;
}

/*@only@*/ cstring
sRef_unparseDebug (sRef s)
{
  if (sRef_isInvalid (s)) return (cstring_makeLiteral ("<undef>"));

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
      return (message ("<unconstrained %s>", s->info->fname));
    case SK_CVAR:
      {
	uentry ce;

	ce = usymtab_getRefQuiet (s->info->cvar->lexlevel, s->info->cvar->index);

	if (uentry_isInvalid (ce))
	  {
	    return (message ("<scope: %d.%d *invalid*>", 
			     s->info->cvar->lexlevel,
			     s->info->cvar->index));
	  }
	else
	  {
	    return (message ("<scope: %d.%d *%q*>", 
			     s->info->cvar->lexlevel,
			     s->info->cvar->index,
			     uentry_getName (ce)));
	  }

      }
    case SK_PARAM:
      {
	return (message ("<parameter %d>", s->info->paramno + 1));
      }
    case SK_ARRAYFETCH:
      if (s->info->arrayfetch->indknown)
	{
	  return (message ("%q[%d]", sRef_unparseDebug (s->info->arrayfetch->arr),
			   s->info->arrayfetch->ind));
	}
      else
	{
	  return (message ("%q[]", sRef_unparseDebug (s->info->arrayfetch->arr)));
	}
    case SK_FIELD:
      return (message ("%q.%s", sRef_unparseDebug (s->info->field->rec),
		       s->info->field->field));
    case SK_PTR:
      return (message ("*(%q)", sRef_unparseDebug (s->info->ref)));
    case SK_ADR:
      return (message ("&%q", sRef_unparseDebug (s->info->ref)));
    case SK_OBJECT:
      return (message ("<object type %s>", ctype_unparse (s->info->object)));
    case SK_CONJ:
      return (message ("%q | %q", sRef_unparseDebug (s->info->conj->a),
		       sRef_unparseDebug (s->info->conj->b)));
    case SK_NEW:
      return message ("<new: %s>", s->info->fname);
    case SK_DERIVED:
      return (message ("<derived %q>", sRef_unparseDebug (s->info->ref)));
    case SK_EXTERNAL:
      return (message ("<external %q>", sRef_unparseDebug (s->info->ref)));
    case SK_TYPE:
      return (message ("<type %s>", ctype_unparse (s->type)));
    case SK_CONST:
      return (message ("<const %s>", ctype_unparse (s->type)));
    case SK_RESULT:
      return (message ("<result %s>", ctype_unparse (s->type)));
    case SK_SPECIAL:
      return (message ("<spec %s>",
		       cstring_makeLiteralTemp
		       (s->info->spec == SR_NOTHING ? "nothing"
			: s->info->spec == SR_INTERNAL ? "internalState"
			: s->info->spec == SR_SPECSTATE ? "spec state"
			: s->info->spec == SR_SYSTEM ? "fileSystem"
			: "error")));
    case SK_UNKNOWN:
      return cstring_makeLiteral ("<unknown>");
    }

  BADEXIT;
}

static /*@only@*/ cstring
sRef_unparseNoArgs (sRef s)
{
  if (sRef_isInvalid (s)) return (cstring_makeLiteral ("?"));

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
      return (cstring_copy (s->info->fname));
    case SK_CVAR:
      {
	uentry ce = usymtab_getRefQuiet (s->info->cvar->lexlevel, 
					 s->info->cvar->index);

	if (uentry_isInvalid (ce))
	  {
	    llcontbug (message ("sRef_unparseNoArgs: bad cvar: %q", sRef_unparseDebug (s)));
	    return (sRef_unparseDebug (s)); 
	  }
	else
	  {
	    return (uentry_getName (ce));
	  }
      }
    case SK_ARRAYFETCH:
      if (s->info->arrayfetch->indknown)
	{
	  return (message ("%q[%d]", sRef_unparseNoArgs (s->info->arrayfetch->arr),
			   s->info->arrayfetch->ind));
	}
      else
	{
	  return (message ("%q[]", sRef_unparseNoArgs (s->info->arrayfetch->arr)));
	}
    case SK_FIELD:
      return (message ("%q.%s", sRef_unparseNoArgs (s->info->field->rec),
		       s->info->field->field));
    case SK_PTR:
      {
	sRef ref = sRef_fixConj (s->info->ref);
	skind sk = ref->kind;
	cstring ret;

	if (skind_isSimple (sk) || sk == SK_PTR)
	  {
	    ret = message ("*%q", sRef_unparseNoArgs (ref));
	  }
	else
	  {
	    ret = message ("*(%q)", sRef_unparseNoArgs (ref));
	  }

	return (ret);
      }
    case SK_ADR:
      return (message ("&%q", sRef_unparseNoArgs (s->info->ref)));
    case SK_OBJECT:
      return (cstring_copy (ctype_unparse (s->info->object)));
    case SK_CONJ:
      return (sRef_unparseNoArgs (s->info->conj->a));
    case SK_NEW:
      return (message ("result of %s", s->info->fname));
    case SK_DERIVED:
      return (message ("<der %q>", sRef_unparseNoArgs (s->info->ref)));
    case SK_EXTERNAL:
      return message ("<ext %q>", sRef_unparseNoArgs (s->info->ref));
    case SK_SPECIAL:
      return (cstring_makeLiteral
	      (s->info->spec == SR_NOTHING ? "nothing"
	       : s->info->spec == SR_INTERNAL ? "internal state"
	       : s->info->spec == SR_SPECSTATE ? "spec state"
	       : s->info->spec == SR_SYSTEM ? "file system state"
	       : "<spec error>"));
    case SK_RESULT:
      return cstring_makeLiteral ("result");
    case SK_CONST:
    case SK_TYPE:
    case SK_UNKNOWN:
      return cstring_makeLiteral ("?");
    case SK_PARAM:
      /* llcontbug (message ("sRef_unparseNoArgs: bad case: %q", sRef_unparseDebug (s))); */
      return (sRef_unparseDebug (s));
    }
  BADEXIT;
}

/*@dependent@*/ sRef sRef_makeUnconstrained (cstring fname)
{
  sRef s = sRef_new ();

  s->kind = SK_UNCONSTRAINED;
  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->fname = fname;

  return (s);
}

cstring sRef_unconstrainedName (sRef s)
{
  llassert (sRef_isUnconstrained (s));

  return s->info->fname;
}

bool sRef_isUnconstrained (sRef s) 
{
  return (sRef_isValid(s) && s->kind == SK_UNCONSTRAINED);
}

static /*@dependent@*/ /*@notnull@*/ sRef 
  sRef_makeCvarAux (int level, usymId index, ctype ct)
{
  sRef s = sRef_new ();

    s->kind = SK_CVAR;
  s->info = (sinfo) dmalloc (sizeof (*s->info));

  s->info->cvar = (cref) dmalloc (sizeof (*s->info->cvar));
  s->info->cvar->lexlevel = level;
  s->info->cvar->index = index;

  /* for now, all globals are defined; all locals, aren't */

  if (level <= fileScope)
    {
      s->defstate = SS_UNKNOWN;
    }
  else 
    {
      ctype rct = ctype_realType (ct);

      if (level != paramsScope
	  && (ctype_isStructorUnion (rct) || ctype_isRealArray (rct)))
	{
	  s->defstate = SS_ALLOCATED; 
	  s->oaliaskind = s->aliaskind = AK_STACK;
	}
      else
	{
	  s->defstate = SS_UNDEFINED;
	  s->oaliaskind = s->aliaskind = AK_LOCAL;
	}
    }

  s->type = ct;

  llassert (level >= globScope);
  llassert (usymId_isValid (index));

  return s;
}

/*@dependent@*/ sRef sRef_makeCvar (int level, usymId index, ctype ct)
{
  return (sRef_makeCvarAux (level, index, ct));
}

int sRef_lexLevel (sRef s)
{
  if (sRef_isValid (s))
    {
      sRef conj;

      conj = sRef_fixConj (s);
      s = sRef_getRootBase (conj);
      
      if (sRef_isValid (s) && s->kind == SK_CVAR)
	{
	  return (s->info->cvar->lexlevel);
	}
    }

  return globScope;
}

sRef
sRef_makeGlobal (usymId l, ctype ct)
{
  return (sRef_makeCvar (globScope, l, ct));
}

void
sRef_setParamNo (sRef s, int l)
{
  llassert (sRef_isValid (s) && s->kind == SK_PARAM);
  s->info->paramno = l;
}

/*@dependent@*/ sRef
sRef_makeParam (int l, ctype ct)
{
  sRef s = sRef_new ();

  s->kind = SK_PARAM;
  s->type = ct;

  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->paramno = l; 
  s->defstate = SS_UNKNOWN; /* (probably defined, unless its an out parameter) */

  return s;
}

bool
sRef_isIndexKnown (sRef arr)
{
  bool res;

  llassert (sRef_isValid (arr));
  arr = sRef_fixConj (arr);
  
  llassert (arr->kind == SK_ARRAYFETCH);  
  res = arr->info->arrayfetch->indknown;
  return (res);
}

int
sRef_getIndex (sRef arr)
{
  int result;

  llassert (sRef_isValid (arr));
  arr = sRef_fixConj (arr);

  llassert (arr->kind == SK_ARRAYFETCH);  

  if (!arr->info->arrayfetch->indknown)
    {
      llcontbug (message ("sRef_getIndex: unknown: %q", sRef_unparse (arr)));
      result = 0; 
    }
  else
    {
      result = arr->info->arrayfetch->ind;
    }

  return result;
}

static bool sRef_isZerothArrayFetch (/*@notnull@*/ sRef s)
{
  return (s->kind == SK_ARRAYFETCH
	  && s->info->arrayfetch->indknown
	  && (s->info->arrayfetch->ind == 0));
}

/*@exposed@*/ sRef sRef_makeAddress (/*@exposed@*/ sRef t)
{
  
  if (sRef_isInvalid (t)) return sRef_undefined;

  if (sRef_isPointer (t))
    {
      return (t->info->ref);
    }
  else if (sRef_isZerothArrayFetch (t))
    {
      return (t->info->arrayfetch->arr);
    }
  else
    {
      sRef s = sRef_new ();
      
      s->kind = SK_ADR;
      s->type = ctype_makePointer (t->type);
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->ref = t;
      
      if (t->defstate == SS_UNDEFINED) 
	/* no! it is allocated even still: && !ctype_isPointer (t->type)) */
	{
	  s->defstate = SS_ALLOCATED;
	}
      else
	{
	  s->defstate = t->defstate;
	}

      if (t->aliaskind == AK_LOCAL)
	{
	  if (sRef_isLocalVar (t))
	    {
	      s->aliaskind = AK_STACK;
	    }
	}

      return s;
    }
}

cstring sRef_getField (sRef s)
{
  cstring res;

  llassert (sRef_isValid (s));
  s = sRef_fixConj (s);

  llassertprint (sRef_isValid (s) && (s->kind == SK_FIELD),
		 ("s = %s", sRef_unparseDebug (s)));

  res = s->info->field->field;
  return (res);
}

sRef sRef_getBase (sRef s)
{
  sRef res;

  if (sRef_isInvalid (s)) return (sRef_undefined);

  s = sRef_fixConj (s);

  switch (s->kind)
    {
    case SK_ADR:
    case SK_PTR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      res = s->info->ref;
      break;
    case SK_FIELD:
      res = s->info->field->rec;
      break;

    case SK_ARRAYFETCH:
      res = s->info->arrayfetch->arr;
      break;

    default:
      res = sRef_undefined; /* shouldn't need it */
    }

  return (res);
}

/*
** same as getBase, except returns invalid
** (and doesn't use adr's)                   
*/

sRef
sRef_getBaseSafe (sRef s)
{
  sRef res;

  if (sRef_isInvalid (s)) { return sRef_undefined; }

  s = sRef_fixConj (s);

  switch (s->kind)
    {
    case SK_PTR:
            res = s->info->ref; 
      break;
    case SK_FIELD:
            res = s->info->field->rec; break;
    case SK_ARRAYFETCH:
            res = s->info->arrayfetch->arr; 
      break;
    default:
      res = sRef_undefined; break;
    }

  return res;
}

/*@constant int MAXBASEDEPTH;@*/
# define MAXBASEDEPTH 25

static /*@exposed@*/ sRef 
sRef_getRootBaseAux (sRef s, int depth)
{
  if (sRef_isInvalid (s)) return sRef_undefined;

  if (depth > MAXBASEDEPTH)
    {
      llgenmsg (message 
		("Warning: reference base limit exceeded for %q. "
		 "This either means there is a variable with at least "
		 "%d indirections from this reference, or "
		 "there is a bug in LCLint.",
		 sRef_unparse (s),
		 MAXBASEDEPTH),
		g_currentloc);

      return sRef_undefined;
    }

  switch (s->kind)
    {
    case SK_ADR:
    case SK_PTR:
      return (sRef_getRootBaseAux (s->info->ref, depth + 1));
    case SK_FIELD:
      return (sRef_getRootBaseAux (s->info->field->rec, depth + 1));
    case SK_ARRAYFETCH:
      return (sRef_getRootBaseAux (s->info->arrayfetch->arr, depth + 1));
    case SK_CONJ:
      return (sRef_getRootBaseAux (sRef_fixConj (s), depth + 1));
    default:
      return s;
    }
}

sRef sRef_getRootBase (sRef s)
{
  return (sRef_getRootBaseAux (s, 0));
}

static bool sRef_isDeep (sRef s)
{
  if (sRef_isInvalid (s)) return FALSE;
  
  switch (s->kind)
    {
    case SK_ADR:
    case SK_PTR:
    case SK_FIELD:
    case SK_ARRAYFETCH:
      return TRUE;
    case SK_CONJ:
      return (sRef_isDeep (sRef_fixConj (s)));
    default:
      return FALSE;
    }
}

static int sRef_depth (sRef s)
{
  if (sRef_isInvalid (s)) return 0;
  
  switch (s->kind)
    {
    case SK_ADR:
    case SK_PTR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      return 1 + sRef_depth (s->info->ref);
    case SK_FIELD:
      return 1 + sRef_depth (s->info->field->rec);
    case SK_ARRAYFETCH:
      return 1 + sRef_depth (s->info->arrayfetch->arr);
    case SK_CONJ:
      return (sRef_depth (sRef_fixConj (s)));
    default:
      return 1;
    }
}

sRef
sRef_makeObject (ctype o)
{
  sRef s = sRef_new ();

  s->kind = SK_OBJECT;
  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->object = o;
  return s;
}

sRef sRef_makeExternal (/*@exposed@*/ sRef t)
{
  sRef s = sRef_new ();

  llassert (sRef_isValid (t));

  s->kind = SK_EXTERNAL;
  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->type = t->type;
  s->info->ref = t;
  return s;
}

sRef sRef_makeDerived (/*@exposed@*/ sRef t)
{
  if (sRef_isValid (t))
    {
      sRef s = sRef_new ();
      
      s->kind = SK_DERIVED;
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->ref = t;
      
      s->type = t->type;
      return s;
    }
  else
    {
      return sRef_undefined;
    }
}

/*
** definitely NOT symmetric:
**
**   res fills in unknown state information from other
*/

void
sRef_mergeStateQuiet (sRef res, sRef other)
{
  llassert (sRef_isValid (res));
  llassert (sRef_isValid (other));

  res->modified = res->modified || other->modified;
  res->safe = res->safe && other->safe;

  if (res->defstate == SS_UNKNOWN) 
    {
      res->defstate = other->defstate;
      res->definfo = alinfo_update (res->definfo, other->definfo);
    }

  if (res->aliaskind == AK_UNKNOWN || 
      (res->aliaskind == AK_LOCAL && alkind_isKnown (other->aliaskind)))
    {
      res->aliaskind = other->aliaskind;
      res->oaliaskind = other->oaliaskind;
      res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
    }

  if (res->expkind == XO_UNKNOWN)
    {
      res->expkind = other->expkind;
      res->oexpkind = other->oexpkind;
      res->expinfo = alinfo_update (res->expinfo, other->expinfo);
    }
  
  /* out takes precedence over implicitly defined */
  if (res->defstate == SS_DEFINED && other->defstate != SS_UNKNOWN) 
    {
      res->defstate = other->defstate;
      res->definfo = alinfo_update (res->definfo, other->definfo);
    }

  if (other->nullstate == NS_ERROR || res->nullstate == NS_ERROR) 
    {
      res->nullstate = NS_ERROR;
    }
  else
    {
      if (other->nullstate != NS_UNKNOWN 
	  && (res->nullstate == NS_UNKNOWN || res->nullstate == NS_NOTNULL 
	      || res->nullstate == NS_MNOTNULL))
	{
	  res->nullstate = other->nullstate;
	  res->nullinfo = alinfo_update (res->nullinfo, other->nullinfo);
	}
    }
}

/*
** definitely NOT symmetric:
**
**   res fills in known state information from other
*/

void
sRef_mergeStateQuietReverse (sRef res, sRef other)
{
  bool changed = FALSE;

  llassert (sRef_isValid (res));
  llassert (sRef_isValid (other));

  if (res->kind != other->kind)
    {
      changed = TRUE;

      sinfo_free (res);

      res->kind = other->kind;
      res->type = other->type;
      res->info = sinfo_fullCopy (other);
    }
  else
    {
      if (!ctype_equal (res->type, other->type))
	{
	  changed = TRUE;
	  res->type = other->type;
	}
      
      sinfo_update (res, other);
    }

  res->modified = res->modified || other->modified;
  res->safe = res->safe && other->safe;

  if (res->aliaskind != other->aliaskind
      && (res->aliaskind == AK_UNKNOWN
	  || ((res->aliaskind == AK_LOCAL 
	       || (res->aliaskind == AK_REFCOUNTED
		   && other->aliaskind != AK_LOCAL))
	      && other->aliaskind != AK_UNKNOWN)))
    {
      changed = TRUE;
      res->aliaskind = other->aliaskind;
      res->oaliaskind = other->oaliaskind;
      res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
          }

  if (other->expkind != XO_UNKNOWN && other->expkind != res->expkind)
    {
      changed = TRUE;
      res->expkind = other->expkind;
      res->expinfo = alinfo_update (res->expinfo, other->expinfo);
    }

  if (other->oexpkind != XO_UNKNOWN)
    {
      res->oexpkind = other->oexpkind;
    }

  /* out takes precedence over implicitly defined */

  if (res->defstate != other->defstate)
    {
      if (other->defstate != SS_UNKNOWN)
	{
	  res->defstate = other->defstate;
	}
    }

  if (other->nullstate == NS_ERROR || res->nullstate == NS_ERROR)
    {
      if (res->nullstate != NS_ERROR)
	{
	  res->nullstate = NS_ERROR;
	  changed = TRUE;
	}
    }
  else
    {
      if (other->nullstate != NS_UNKNOWN && other->nullstate != res->nullstate)
	{
	  changed = TRUE;
	  res->nullstate = other->nullstate;
	  res->nullinfo = alinfo_update (res->nullinfo, other->nullinfo);
	}
    }

  if (changed)
    {
      sRef_clearDerived (res); 
    }
}

void 
sRef_mergeState (sRef res, sRef other, clause cl, fileloc loc)
{
  if (sRef_isValid (res) && sRef_isValid (other))
    {
      sRef_mergeStateAux (res, other, cl, FALSE, loc, TRUE);
    }
  else
    {
      if (sRef_isInvalid (res))
	{
	  llbug (message ("sRef_mergeState: invalid res sRef: %q", 
			  sRef_unparseDebug (other)));
	}
      else 
	{
	  llbug (message ("sRef_mergeState: invalid other sRef: %q", 
			  sRef_unparseDebug (res)));
	}
    }
}

void 
sRef_mergeOptState (sRef res, sRef other, clause cl, fileloc loc)
{
  if (sRef_isValid (res) && sRef_isValid (other))
    {
      sRef_mergeStateAux (res, other, cl, TRUE, loc, TRUE);
    }
  else
    {
      if (sRef_isInvalid (res))
	{
	  llbug (message ("sRef_mergeOptState: invalid res sRef: %q", 
			  sRef_unparseDebug (other)));
	}
      else 
	{
	  llbug (message ("sRef_mergeOptState: invalid other sRef: %q", 
			  sRef_unparseDebug (res)));
	}
    }
}

static void
sRef_mergeStateAux (/*@notnull@*/ sRef res, /*@notnull@*/ sRef other, 
		    clause cl, bool opt, fileloc loc,
		    bool doDerivs)
   /*@modifies res@*/ 
{
  llassertfatal (sRef_isValid (res));
  llassertfatal (sRef_isValid (other));
  
  res->modified = res->modified || other->modified;

  if (res->kind == other->kind 
      || (other->kind == SK_UNKNOWN || res->kind == SK_UNKNOWN))
    {
      sstate odef = other->defstate;
      sstate rdef = res->defstate;
      nstate onull = other->nullstate;
      
      /*
      ** yucky stuff to handle 
      **
      **   if (s) free (s);
      */

      if (other->defstate == SS_DEAD 
	  && ((sRef_isOnly (res) && sRef_definitelyNull (res))
	      || (res->defstate == SS_UNDEFINED
		  || res->defstate == SS_UNUSEABLE)))
	{
	  if (res->defstate == SS_UNDEFINED
	      || res->defstate == SS_UNUSEABLE)
	    {
	      res->defstate = SS_UNUSEABLE;
	    }
	  else
	    {
	      res->defstate = SS_DEAD;
	    }

	  res->definfo = alinfo_update (res->definfo, other->definfo);
	  sRef_clearDerived (other);
	  sRef_clearDerived (res);
	}
      else if (res->defstate == SS_DEAD 
	       && ((sRef_isOnly (other) && sRef_definitelyNull (other))
		   || (other->defstate == SS_UNDEFINED
		       || other->defstate == SS_UNUSEABLE)))
	{
	  if (other->defstate == SS_UNDEFINED
	      || other->defstate == SS_UNUSEABLE)
	    {
	      res->defstate = SS_UNUSEABLE;
	    }
	  else
	    {
	      res->defstate = SS_DEAD;
	    }
	  
	  sRef_clearDerived (other);
	  sRef_clearDerived (res);
	}
      else if (res->defstate == SS_DEFINED 
	       && (other->defstate == SS_ALLOCATED 
		   && sRef_definitelyNull (other)))
	{
	  other->defstate = SS_DEFINED; /* definitely null! */
	}
      else if (other->defstate == SS_DEFINED
	       && (res->defstate == SS_ALLOCATED && sRef_definitelyNull (res)))
	{
	  res->defstate = SS_DEFINED;
	  res->definfo = alinfo_update (res->definfo, other->definfo);
	}
      else
	{
	  ; /* okay */
	}

      if (res->defstate == SS_DEAD && other->defstate == SS_DEAD)
	{
	  sRef_clearDerived (other);
	  sRef_clearDerived (res);
	}

      /*
      ** only & dead isn't really an only!
      */

      if (alkind_isOnly (other->aliaskind) && other->defstate == SS_DEAD)
	{
	  other->aliaskind = AK_UNKNOWN;
	}

      if (alkind_isOnly (res->aliaskind) && res->defstate == SS_DEAD)
	{
	  res->aliaskind = AK_UNKNOWN;
	}

      /*
      ** Dead and dependent -> dead
      */
      
      if (alkind_isDependent (other->aliaskind) && res->defstate == SS_DEAD)
	{
	  other->aliaskind = AK_UNKNOWN;
	  other->defstate = SS_DEAD;
	  sRef_clearDerived (res);
	  sRef_clearDerived (other);
	  	}

      if (alkind_isDependent (res->aliaskind) && other->defstate == SS_DEAD)
	{
	  res->aliaskind = AK_UNKNOWN;
	  res->defstate = SS_DEAD;
	  sRef_clearDerived (res);
	  sRef_clearDerived (other);
	}

      /*
      ** must do alias combine first, since it depends on 
      ** original values of state and null.
      */

      sRef_combineAliasKinds (res, other, cl, loc);
      sRef_combineDefState (res, other);
      sRef_combineNullState (res, other);

      if (rdef == SS_ALLOCATED || rdef == SS_SPECIAL)
	{
	  if (odef == SS_DEFINED)
	    {
	      if (onull == NS_DEFNULL || onull == NS_CONSTNULL)
		{
		  res->deriv = sRefSet_copy (res->deriv, other->deriv);
		}

	      	      	      ; 
	    }
	  else if (odef == SS_ALLOCATED
		   || odef == SS_SPECIAL)
	    {
	      
	      if (doDerivs)
		{
		  if (ctype_isUnion (ctype_realType (sRef_getType (res))))
		    {
		      res->deriv = sRef_mergeUnionDerivs (res->deriv, 
							  other->deriv, 
							  opt, cl, loc);
		    }
		  else
		    {
		      		      res->deriv = sRef_mergeDerivs (res->deriv, other->deriv, 
						     opt, cl, loc);
		    }
		}
	    }
	  else
	    {
	      if (doDerivs)
		{
		  		  res->deriv = sRef_mergeDerivs (res->deriv, other->deriv, 
						 opt, cl, loc);
		}
	      else
		{
		  		}
	    }
	}
      else
	{
	  if (rdef == SS_PDEFINED
	      || (rdef == SS_DEFINED && odef == SS_PDEFINED))
	    {
	      if (doDerivs)
		{
		  		  res->deriv = sRef_mergePdefinedDerivs (res->deriv, other->deriv, 
							 opt, cl, loc);
		}
	    }
	  else
	    {
	      if ((rdef == SS_DEFINED  || rdef == SS_UNKNOWN)
		  && res->defstate == SS_ALLOCATED)
		{
		  		  res->deriv = sRefSet_copy (res->deriv, other->deriv);
		}
	      else
		{
		  if (doDerivs)
		    {
		      		      res->deriv = sRef_mergeDerivs (res->deriv, other->deriv, 
						     opt, cl, loc);
		    }
		}
	    }
	}

      
      sRef_combineExKinds (res, other);
    }
  else
    {
      if (res->kind == SK_ARRAYFETCH && other->kind == SK_PTR)
	{
	  sRef nother = sRef_buildArrayFetchKnown (sRef_getBase (other), 0);

	  sRef_copyState (nother, other);
	  sRef_mergeStateAux (res, nother, cl, opt, loc, doDerivs);
	}
      else if (res->kind == SK_PTR && other->kind == SK_ARRAYFETCH)
	{
	  sRef nother = sRef_buildPointer (sRef_getBase (other));

	  if (sRef_isValid (nother))
	    {
	      sRef_copyState (nother, other);
	      sRef_mergeStateAux (res, nother, cl, opt, loc, doDerivs);
	    }
	}
      else
	{
	  llcontbug (message ("merge conj: %q / %q", sRef_unparseFull (res), 
			      sRef_unparseFull (other)));
	  
	}
    }
  
  }

static sRefSet
sRef_mergeUnionDerivs (/*@only@*/ sRefSet res, 
		       /*@exposed@*/ sRefSet other, bool opt,
		       clause cl, fileloc loc)
{
  if (sRefSet_isEmpty (res))
    {
      return sRefSet_copy (res, other);
    }
  else
    {
      sRefSet_allElements (other, el)
	{
	  if (sRef_isValid (el))
	    {
	      sRef e2 = sRefSet_lookupMember (other, el);
	      
	      if (sRef_isValid (e2))
		{
		  sRef_mergeStateAux (el, e2, cl, opt, loc, FALSE);
		}
	      else
		{
		  res = sRefSet_insert (res, el);
		}
	    }
	} end_sRefSet_allElements ;

      return res;
    }
}

static /*@only@*/ sRefSet
sRef_mergeDerivs (/*@only@*/ sRefSet res, sRefSet other, 
		  bool opt, clause cl, fileloc loc)
{
  sRefSet ret = sRefSet_new ();
  
    
  sRefSet_allElements (res, el)
    {
      if (sRef_isValid (el))
	{
	  sRef e2 = sRefSet_lookupMember (other, el);

	  if (sRef_isValid (e2))
	    {
	      if (el->defstate == SS_ALLOCATED &&
		  e2->defstate == SS_PDEFINED)
		{
		  e2->defstate = SS_ALLOCATED;
		}
	      else if (e2->defstate == SS_ALLOCATED &&
		       el->defstate == SS_PDEFINED)
		{
		  el->defstate = SS_ALLOCATED;
		  sRef_clearDerived (el);
		}
	      else if ((el->defstate == SS_DEAD || sRef_isKept (el)) &&
		       (e2->defstate == SS_DEFINED && !sRef_isKept (e2)))
		{
		  
		  if (checkDeadState (el, TRUE, loc))
		    {
		      if (sRef_isThroughArrayFetch (el))
			{
			  sRef_maybeKill (el, loc);
			  sRef_maybeKill (e2, loc);
			}
		    }
		}
	      else if ((e2->defstate == SS_DEAD || sRef_isKept (e2)) &&
		       (el->defstate == SS_DEFINED && !sRef_isKept (el)))
		{
		  
		  if (checkDeadState (e2, FALSE, loc))
		    {
		      if (sRef_isThroughArrayFetch (el))
			{
			  sRef_maybeKill (el, loc);
			  sRef_maybeKill (e2, loc);
			}
		    }
		}
	      else if (el->defstate == SS_DEFINED &&
		       e2->defstate == SS_PDEFINED)
		{
		  el->defstate = SS_PDEFINED;
		}
	      else if (e2->defstate == SS_DEFINED &&
		       el->defstate == SS_PDEFINED)
		{
		  e2->defstate = SS_PDEFINED;
		}
	      else
		{
		  ; /* okay */
		}

	      if (ctype_isUnion (ctype_realType (sRef_getType (el))))
		{
		  el->deriv = sRef_mergeUnionDerivs (el->deriv, e2->deriv, 
						     opt, cl, loc); 
		}
	      else
		{
		  el->deriv = sRef_mergeDerivs (el->deriv, e2->deriv, opt, cl, loc); 
		}
	      
	      if (sRef_equivalent (el, e2))
		{
		  		  ret = sRefSet_insert (ret, el);
		}
	      else
		{
		  sRef sr = sRef_leastCommon (el, e2);

		  if (sRef_isValid (sr))
		    {
		      ret = sRefSet_insert (ret, sr);
		    }
		  else
		    {
		      ;
		    }
		}
	      
	      (void) sRefSet_delete (other, e2);
	    }
	  else /* not defined */
	    {
	      	      (void) checkDeadState (el, TRUE, loc);
	    }
	}
    } end_sRefSet_allElements;

  sRefSet_allElements (other, el)
    {
      if (sRef_isValid (el))
	{
	  (void) checkDeadState (el, FALSE, loc);
	}
    } end_sRefSet_allElements;
  
  sRefSet_free (res); 
  return (ret);
}

/*
** Returns TRUE is there is an error.
*/

static bool checkDeadState (/*@notnull@*/ sRef el, bool tbranch, fileloc loc)
{
  /*
  ** usymtab_isGuarded --- the utab should still be in the
  ** state of the alternate branch.
  **
  ** tbranch TRUE means el is released in the last branch, e.g.
  **     if (x != NULL) { ; } else { sfree (x); }
  ** so, if x is null in the other branch no error is reported.
  **
  ** tbranch FALSE means this is the other branch:
  **     if (x != NULL) { sfree (x); } else { ; }
  ** so, if x is null in this branch there is no error.
  */

  
  if ((sRef_isDead (el) || sRef_isKept (el))
      && !sRef_isDeepUnionField (el) && !sRef_isThroughArrayFetch (el))
    {
       
      if (!tbranch)
	{
	  if (usymtab_isProbableDeepNull (el))
	    {
	      	      return TRUE;
	    }
	}
      else
	{
	  if (usymtab_isAltProbablyDeepNull (el))
	    {
	      	      return TRUE;
	    }
	}

      if (optgenerror
	  (FLG_BRANCHSTATE,
	   message ("Storage %q is %q in one path, but live in another.",
		    sRef_unparse (el),
		    cstring_makeLiteral (sRef_isKept (el) 
					 ? "kept" : "released")),
	   loc))
	{
	  	  
	  if (sRef_isKept (el))
	    {
	      sRef_showAliasInfo (el);      
	    }
	  else
	    {
	      sRef_showStateInfo (el);
	    }

	  /* prevent further errors */
	  el->defstate = SS_UNKNOWN; 
	  sRef_setAliasKind (el, AK_ERROR, fileloc_undefined);
	  
	  return FALSE;
	}
    }

  return TRUE;
}

static void 
checkDerivDeadState (/*@notnull@*/ sRef el, bool tbranch, fileloc loc)
{
  
  if (checkDeadState (el, tbranch, loc))
    {
      sRefSet_allElements (el->deriv, t)
	{
	  if (sRef_isValid (t))
	    {
	      	      checkDerivDeadState (t, tbranch, loc);
	    }
	} end_sRefSet_allElements;
    }
}

static sRefSet
  sRef_mergePdefinedDerivs (sRefSet res, sRefSet other, bool opt, 
			    clause cl, fileloc loc)
{
  sRefSet ret = sRefSet_new ();

  sRefSet_allElements (res, el)
    {
      if (sRef_isValid (el))
	{
	  sRef e2 = sRefSet_lookupMember (other, el);
	  
	  if (sRef_isValid (e2))
	    {
	      if (sRef_isAllocated (el) && !sRef_isAllocated (e2))
		{
		  ;
		}
	      else if (sRef_isAllocated (e2) && !sRef_isAllocated (el))
		{
		  el->deriv = sRefSet_copy (el->deriv, e2->deriv); 
		}
	      else
		{
		  el->deriv = sRef_mergePdefinedDerivs (el->deriv, e2->deriv, 
							opt, cl, loc);
		}

	      sRef_mergeStateAux (el, e2, cl, opt, loc, FALSE);
	      
	      ret = sRefSet_insert (ret, el);
	      (void) sRefSet_delete (other, e2);
	    }
	  else
	    {
	      if (!opt)
		{
		  		  checkDerivDeadState (el, (cl == FALSECLAUSE), loc);
		}

	      ret = sRefSet_insert (ret, el);
	    }
	}
    } end_sRefSet_allElements;
  
  sRefSet_allElements (other, el)
    {
      if (sRef_isValid (el))
	{
	  if (!sRefSet_member (ret, el))
	    {
	      	      	      /* was cl == FALSECLAUSE */
	      checkDerivDeadState (el, FALSE, loc);
	      ret = sRefSet_insert (ret, el);
	    }
	  else
	    {
	      /*
	      ** it's okay --- member is a different equality test 
	      */
	    }
	}
    } end_sRefSet_allElements;

  sRefSet_free (res);
  return (ret);
}

sRef sRef_makeConj (/*@exposed@*/ /*@returned@*/ sRef a, /*@exposed@*/ sRef b)
{
  llassert (sRef_isValid (a));
  llassert (sRef_isValid (b));
      
  if (!sRef_equivalent (a, b))
    {
      sRef s = sRef_new ();
      
      s->kind = SK_CONJ;
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->conj = (cjinfo) dmalloc (sizeof (*s->info->conj));
      s->info->conj->a = a;
      s->info->conj->b = b;
      
      if (ctype_equal (a->type, b->type)) s->type = a->type;
      else s->type = ctype_makeConj (a->type, b->type);
      
      if (a->defstate == b->defstate)
	{
	  s->defstate = a->defstate;
	}
      else
	{
	  s->defstate = SS_UNKNOWN; 
	}
      
      s->nullstate = NS_UNKNOWN;
      
      s->safe = a->safe && b->safe;
      s->aliaskind = alkind_resolve (a->aliaskind, b->aliaskind);

      return s;
    }
  else
    {
      /*@-exposetrans@*/ return a; /*@=exposetrans@*/
    }
}

sRef
sRef_makeUnknown ()
{
  sRef s = sRef_new ();

  s->kind = SK_UNKNOWN;
  return s;
}

static sRef
sRef_makeSpecial (speckind sk) /*@*/
{
  sRef s = sRef_new ();

  s->kind = SK_SPECIAL;
  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->spec = sk;
  return s;
}

static sRef srnothing = sRef_undefined;
static sRef srinternal = sRef_undefined;
static sRef srsystem = sRef_undefined;
static sRef srspec = sRef_undefined;

sRef
sRef_makeNothing (void)
{
  if (sRef_isInvalid (srnothing))
    {
      srnothing = sRef_makeSpecial (SR_NOTHING);
    }

  /*@-retalias@*/
  return srnothing;
  /*@=retalias@*/
}

sRef
sRef_makeInternalState (void)
{
  if (sRef_isInvalid (srinternal))
    {
      srinternal = sRef_makeSpecial (SR_INTERNAL);
    }

  /*@-retalias@*/
  return srinternal;
  /*@=retalias@*/
}

sRef
sRef_makeSpecState (void)
{
  if (sRef_isInvalid (srspec))
    {
      srspec = sRef_makeSpecial (SR_SPECSTATE);
    }

  /*@-retalias@*/
  return srspec;
  /*@=retalias@*/
}

sRef
sRef_makeSystemState (void)
{
  if (sRef_isInvalid (srsystem))
    {
      srsystem = sRef_makeSpecial (SR_SYSTEM);
    }

  /*@-retalias@*/
  return (srsystem);
  /*@=retalias@*/
}

static sRef
sRef_makeResultType (ctype ct)
{
  sRef res = sRef_makeResult ();

  res->type = ct;
  return res;
}

sRef
sRef_makeResult ()
{
  sRef s = sRef_new ();
  
  s->kind = SK_RESULT;
  s->type = ctype_unknown;
  s->defstate = SS_UNKNOWN; 
  s->aliaskind = AK_UNKNOWN;
  s->nullstate = NS_UNKNOWN;
  
  return s;
}


bool
sRef_isNothing (sRef s)
{
  return (sRef_isKindSpecial (s) && s->info->spec == SR_NOTHING);
}

bool
sRef_isInternalState (sRef s)
{
  return (sRef_isKindSpecial (s) && s->info->spec == SR_INTERNAL);
}

bool
sRef_isSpecInternalState (sRef s)
{
  return (sRef_isKindSpecial (s) 
	  && (s->info->spec == SR_INTERNAL || s->info->spec == SR_SPECSTATE));
}

bool
sRef_isSpecState (sRef s)
{
  return (sRef_isKindSpecial (s) && s->info->spec == SR_SPECSTATE);
}

bool
sRef_isResult (sRef s)
{
  return (sRef_isValid (s) && s->kind == SK_RESULT);
}

bool
sRef_isSystemState (sRef s)
{
  return (sRef_isKindSpecial (s) && s->info->spec == SR_SYSTEM);
}

usymId
sRef_getScopeIndex (sRef s)
{
  llassert (sRef_isValid (s));
  llassert (sRef_isCvar (s));

  return (s->info->cvar->index);
}

void
sRef_makeSafe (sRef s)
{
  if (sRef_isValid (s)) 
    {
      s->safe = TRUE;
    }
}

void
sRef_makeUnsafe (sRef s)
{
  if (sRef_isValid (s)) 
    {
      s->safe = FALSE;
    }
}

/*
** memory state operations
*/

/*@only@*/ cstring sRef_unparseFull (sRef s)
{
  if (sRef_isInvalid (s)) return (cstring_undefined);

  return (message ("[%d] %q - %q [%s] { %q }", 
		   (int) s,
		   sRef_unparseDebug (s), 
		   sRef_unparseState (s),
		   exkind_unparse (s->oexpkind),
		   sRefSet_unparseDebug (s->deriv)));
}

/*@unused@*/ cstring sRef_unparseDeep (sRef s)
{
  cstring st = cstring_undefined;

  st = message ("%q:", sRef_unparseFull (s));

  if (sRef_isValid (s))
    {
      sRefSet_allElements (s->deriv, el)
	{
	  st = message("%q\n%q", st, sRef_unparseDeep (el));
	} end_sRefSet_allElements ;
    }

  return st;
}

/*@only@*/ cstring sRef_unparseState (sRef s)
{
  if (sRef_isConj (s))
    {
      return (message ("%q | %q", 
		       sRef_unparseState (s->info->conj->a),
		       sRef_unparseState (s->info->conj->b)));
    }

  if (sRef_isInvalid (s))
    {
      return (cstring_makeLiteral ("<invalid>"));
    }

  return (message ("%s.%s.%s.%s", 
		   alkind_unparse (s->aliaskind), 
		   nstate_unparse (s->nullstate),
		   exkind_unparse (s->expkind),
		   sstate_unparse (s->defstate)));
}

bool sRef_isNotUndefined (sRef s)
{
  return (sRef_isInvalid (s)
	  || (s->defstate != SS_UNDEFINED
	      && s->defstate != SS_UNUSEABLE
	      && s->defstate != SS_DEAD));
}

ynm sRef_isWriteable (sRef s)
{
  if (sRef_isInvalid (s)) return MAYBE;

  if (sRef_isConj (s) && s->defstate == SS_UNKNOWN)
    {
      if (ynm_toBoolStrict (sRef_isWriteable (sRef_getConjA (s))))
	{
	  if (ynm_toBoolStrict (sRef_isWriteable (sRef_getConjB (s))))
	    {
	      return YES;
	    }
	  return MAYBE;
	}
      else
	{
	  if (ynm_toBoolStrict (sRef_isWriteable (sRef_getConjB (s))))
	    {
	      return MAYBE;
	    }
	  return NO;
	}
    }

  return (ynm_fromBool (s->defstate != SS_UNUSEABLE));
}

bool sRef_hasNoStorage (sRef s)
{
  return (!sRef_isAllocatedStorage (s) || sRef_isDefinitelyNull (s));
}

bool sRef_isStrictReadable (sRef s)
{
  return (ynm_toBoolStrict (sRef_isReadable (s)));
}

ynm sRef_isReadable (sRef s)
{
  sstate ss;

  if (sRef_isInvalid (s)) return YES;

  ss = s->defstate;
  
  if (sRef_isConj (s) && s->defstate == SS_UNKNOWN)
    {
      if (ynm_toBoolStrict (sRef_isReadable (sRef_getConjA (s))))
	{
	  if (ynm_toBoolStrict (sRef_isReadable (sRef_getConjB (s))))
	    {
	      return YES;
	    }
	  return MAYBE;
	}
      else
	{
	  if (ynm_toBoolStrict (sRef_isReadable (sRef_getConjB (s))))
	    {
	      return MAYBE;
	    }
	  return NO;
	}
    }
  else if (ss == SS_HOFFA)
    {
      if (context_getFlag (FLG_STRICTUSERELEASED))
	{
	  return MAYBE;
	}
      else
	{
	  return YES;
	}
    }
  else
    {
      return (ynm_fromBool (ss == SS_DEFINED 
			    || ss == SS_FIXED 
			    || ss == SS_RELDEF 
			    || ss == SS_PDEFINED 
			    || ss == SS_PARTIAL
			    || ss == SS_SPECIAL
			    || ss == SS_ALLOCATED
			    || ss == SS_UNKNOWN));
    }
}

static /*@exposed@*/ sRef whatUndefined (/*@exposed@*/ sRef fref, int depth)
{
  ctype ct;

  
  if (depth > MAXDEPTH)
    {
      llgenmsg (message 
		("Warning: check definition limit exceeded, checking %q. "
		 "This either means there is a variable with at least "
		 "%d indirections apparent in the program text, or "
		 "there is a bug in LCLint.",
		 sRef_unparse (fref),
		 MAXDEPTH),
		g_currentloc);

      return sRef_undefined;
    }

  if (!sRef_isKnown (fref) || sRef_isAnyDefined (fref))
    {
      return sRef_undefined;
    }

  if (sRef_isUnuseable (fref) || sRef_isStateUndefined (fref))
    {
      return fref;
    }

  ct = ctype_realType (sRef_getType (fref));
  
  if (ctype_isUnknown (ct))
    {
      return sRef_undefined;
    }
  else if (ctype_isPointer (ct) || ctype_isArray (ct))
    {
      if (sRef_isStateUnknown (fref))
	{
	  return sRef_undefined;
	}
      else
	{
	  sRef fptr = sRef_constructDeref (fref);

	  return (whatUndefined (fptr, depth + 1));
	}
    }
  else if (ctype_isStruct (ct))
    {
      bool hasOneDefined = FALSE;
      
      if (sRef_isStateUnknown (fref))
	{
	  return fref;
	}
	  
      if (sRef_isPdefined (fref) || sRef_isAnyDefined (fref))
	{
	  sRefSet_realElements (sRef_derivedFields (fref), sr)
	    {
	      hasOneDefined = TRUE;
	      
	      if (sRef_isField (sr))
		{
		  cstring fieldname = sRef_getField (sr);
		  sRef fldref = sRef_makeField (fref, fieldname);
		  bool shouldCheck = !sRef_isRecursiveField (fldref);
		  
		  if (shouldCheck)
		    {
		      sRef wdef = whatUndefined (fldref, depth + 1);

		      if (sRef_isValid (wdef))
			{
			  return wdef;
			}
		    }
		}
	    } end_sRefSet_realElements;
	}
      else if (sRef_isAllocated (fref))
	{
	  /*
	  ** for structures, each field must be completely defined
	  */
	  
	  uentryList fields = ctype_getFields (ct);
	      
	  uentryList_elements (fields, ue)
	    {
	      cstring name = uentry_getRealName (ue);
	      sRef ffield = sRef_makeField (fref, name);
	      bool shouldCheck = !sRef_isRecursiveField (ffield);

	      if (sRef_isRelDef (uentry_getSref (ue)))
		{
		  ; /* no error */
		}
	      else
		{
		  if (shouldCheck)
		    {
		      sRef wdef = whatUndefined (ffield, depth + 1);

		      if (sRef_isInvalid (wdef))
			{
			  return wdef;
			}
		    }
		}
	    } end_uentryList_elements;
	}
      else
	{
	  ;
	}
    }
  else if (ctype_isUnion (ct))
    {
      ; 
    }
  else
    {
      ;
    }

  return sRef_undefined;
}

static bool checkDefined (sRef sr)
{
  return (sRef_isInvalid (whatUndefined (sr, 0)));
}

bool sRef_isReallyDefined (sRef s)
{
  if (sRef_isValid (s))
    {
      if (sRef_isAnyDefined (s))
	{
	  return TRUE;
	}
      else
	{
	  if (sRef_isAllocated (s) || sRef_isPdefined (s))
	    {
	      return checkDefined (s);
	    }
	  else
	    {
	      return FALSE;
	    }
	}
    }
  else
    {
      return TRUE;
    }
}

void sRef_showNotReallyDefined (sRef s)
{
  if (sRef_isValid (s))
    {
      if (sRef_isAnyDefined (s))
	{
	  BADBRANCH;
	}
      else
	{
	  if (sRef_isAllocated (s) || sRef_isPdefined (s))
	    {
	      sRef ref = whatUndefined (s, 0);

	      llassert (sRef_isValid (ref));

	      if (ref != s)
		{
		  llgenindentmsgnoloc
		    (message ("This sub-reference is %s: %q",
			      sstate_unparse (sRef_getDefState (ref)),
			      sRef_unparse (ref)));
		}
	    }
	  else
	    {
	      ;
	    }
	}
    }
  else
    {
      BADBRANCH;
    }
}

sstate sRef_getDefState (sRef s)
{
  if (sRef_isInvalid (s)) return (SS_UNKNOWN);
  return (s->defstate);
}

void sRef_setDefState (sRef s, sstate defstate, fileloc loc)
{
  sRef_setStateAux (s, defstate, loc);
}

static void sRef_clearAliasStateAux (sRef s, fileloc loc)
{
  sRef_setAliasKind (s, AK_ERROR, loc);
}

void sRef_clearAliasState (sRef s, fileloc loc)
{
  sRef_aliasSetComplete (sRef_clearAliasStateAux, s, loc);
}

void sRef_setAliasKindComplete (sRef s, alkind kind, fileloc loc)
{
  sRef_aliasSetCompleteParam (sRef_setAliasKind, s, kind, loc);
}

void sRef_setAliasKind (sRef s, alkind kind, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_clearDerived (s);

      if ((kind != s->aliaskind && kind != s->oaliaskind)
	  && fileloc_isDefined (loc))
	{
	  s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
	}
      
      s->aliaskind = kind;
    }
}

void sRef_setOrigAliasKind (sRef s, alkind kind)
{
  if (sRef_isValid (s))
    {
      s->oaliaskind = kind;
    }
}

exkind sRef_getExKind (sRef s)
{
  if (sRef_isValid (s))
    {
      return (s->expkind);
    }
  else
    {
      return XO_UNKNOWN;
    }
}

exkind sRef_getOrigExKind (sRef s)
{
  if (sRef_isValid (s))
    {
      return (s->oexpkind);
    }
  else
    {
      return XO_UNKNOWN;
    }
}

static void sRef_clearExKindAux (sRef s, fileloc loc)
{
  sRef_setExKind (s, XO_UNKNOWN, loc);
}

void sRef_setObserver (sRef s, fileloc loc) 
{
  sRef_setExKind (s, XO_OBSERVER, loc);
}

void sRef_setExposed (sRef s, fileloc loc) 
{
  sRef_setExKind (s, XO_EXPOSED, loc);
}

void sRef_clearExKindComplete (sRef s, fileloc loc)
{
  (void) sRef_aliasSetComplete (sRef_clearExKindAux, s, loc);
}

void sRef_setExKind (sRef s, exkind exp, fileloc loc)
{
  if (sRef_isValid (s))
    {
      if (s->expkind != exp)
	{
	  s->expinfo = alinfo_updateLoc (s->expinfo, loc);
	}
      
      s->expkind = exp;
    }
}

/*
** s1->derived = s2->derived
*/

static void sRef_copyRealDerived (sRef s1, sRef s2)
{
  if (sRef_isValid (s1) && sRef_isValid (s2))
    {
      sRef sb = sRef_getRootBase (s1);

      sRefSet_clear (s1->deriv);

      sRefSet_allElements (s2->deriv, el)
	{
	  if (sRef_isValid (el))
	    {
	      sRef rb = sRef_getRootBase (el);
	      
	      if (!sRef_same (rb, sb))
		{
		  sRef fb = sRef_fixDirectBase (el, s1);
		  
		  if (sRef_isValid (fb))
		    {
		      sRef_copyRealDerived (fb, el);
		      sRef_addDeriv (s1, fb);
		    }
		}
	      else
		{
		  sRef_addDeriv (s1, el);
		}
	    }
	} end_sRefSet_allElements ;
    }
  
  }

void sRef_copyRealDerivedComplete (sRef s1, sRef s2)
{
  sRef_innerAliasSetCompleteParam (sRef_copyRealDerived, s1, s2);
}

void sRef_setUndefined (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      s->defstate = SS_UNDEFINED;

      if (fileloc_isDefined (loc))
	{
	  s->definfo = alinfo_updateLoc (s->definfo, loc);
	}

      sRef_clearDerived (s);
    }
}

static void sRef_setDefinedAux (sRef s, fileloc loc, bool clear)
{
  if (sRef_isInvalid (s)) return;

  if (s->defstate != SS_DEFINED && fileloc_isDefined (loc))
    {
      s->definfo = alinfo_updateLoc (s->definfo, loc);
    }
  
  s->defstate = SS_DEFINED;
  
  /* e.g., if x is allocated, *x = 3 defines x */
  
  if (s->kind == SK_PTR)
    {
      sRef p = s->info->ref;
      
      if (p->defstate == SS_ALLOCATED)
	{
	  sRef_setDefinedAux (p, loc, clear);
	}
    }
  else if (s->kind == SK_ARRAYFETCH) 
    {
      if (!s->info->arrayfetch->indknown
	  || (s->info->arrayfetch->ind == 0))
	{
	  sRef p = s->info->arrayfetch->arr;
	  sRef ptr = sRef_constructPointer (p);
	  
	  if (sRef_isValid (ptr))
	    {
	      if (ptr->defstate == SS_ALLOCATED 
		  || ptr->defstate == SS_UNDEFINED)
		{
		  sRef_setDefinedAux (ptr, loc, clear);
		}
	    }
	  
	  if (p->defstate == SS_RELDEF) 
	    {
	      ;
	    }
	  else if (p->defstate == SS_ALLOCATED || p->defstate == SS_PDEFINED)
	    {
	      p->defstate = SS_DEFINED;
	    }
	  else
	    {
	    }
	}
    }
  else if (s->kind == SK_FIELD)
    {
      sRef parent = s->info->field->rec;
      
      if (sRef_isValid (parent))
	{
	  if (ctype_isUnion (ctype_realType (parent->type)))
	    {
	      /*
	      ** Should not clear derived from here.
	      */
	      
	      sRef_setDefinedNoClear (parent, loc);
	    }
	  else
	    {
	      ; /* Nothing to do for structures. */
	    }
	}

          }
  else
    {
      ;
    }

  if (clear)
    {
      sRef_clearDerived (s);
    }  
}

static void sRef_setPartialDefined (sRef s, fileloc loc)
{
  if (!sRef_isPartial (s))
    {
      sRef_setDefined (s, loc);
    }
}

void sRef_setPartialDefinedComplete (sRef s, fileloc loc)
{
  sRef_innerAliasSetComplete (sRef_setPartialDefined, s, loc);
}

void sRef_setDefinedComplete (sRef s, fileloc loc)
{
  sRef_innerAliasSetComplete (sRef_setDefined, s, loc);
}

void sRef_setDefined (sRef s, fileloc loc)
{
  sRef_setDefinedAux (s, loc, TRUE);
}

static void sRef_setDefinedNoClear (sRef s, fileloc loc)
{
  DPRINTF (("Defining: %s", sRef_unparseFull (s)));
  sRef_setDefinedAux (s, loc, FALSE);
  DPRINTF (("==> %s", sRef_unparseFull (s)));
}

void sRef_setDefinedNCComplete (sRef s, fileloc loc)
{
  DPRINTF (("Set Defined Complete: %s", sRef_unparseFull (s)));
  sRef_innerAliasSetComplete (sRef_setDefinedNoClear, s, loc);
  DPRINTF (("==> %s", sRef_unparseFull (s)));
}

static bool sRef_isDeepUnionField (sRef s)
{
  return (sRef_deepPred (sRef_isUnionField, s));
}

bool sRef_isUnionField (sRef s)
{
  if (sRef_isValid (s) && s->kind == SK_FIELD)
    {
      /*
       ** defining one field of a union defines the union
       */
      
      sRef base = s->info->field->rec;

      if (sRef_isValid (base))
	{
	  return (ctype_isUnion (ctype_realType (base->type)));
	}
    }

  return FALSE;
}

void sRef_setPdefined (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && !sRef_isPartial (s))
    {
      sRef base = sRef_getBaseSafe (s);

      if (s->defstate == SS_ALLOCATED)
	{
	  return;
	}
      
      if (s->defstate != SS_PDEFINED && fileloc_isDefined (loc))
	{
	  s->definfo = alinfo_updateLoc (s->definfo, loc);
	}

      s->defstate = SS_PDEFINED;
      
      /* e.g., if x is allocated, *x = 3 defines x */
      
      while (sRef_isValid (base) && sRef_isKnown (base))
	{
	  if (base->defstate == SS_DEFINED)
	    { 
	      sRef nb;

	      	      base->defstate = SS_PDEFINED; 
	      nb = sRef_getBaseSafe (base); 
	      base = nb;
	    }
	  else 
	    { 
	      break; 
	    }
	}      
    }
}

static void sRef_setStateAux (sRef s, sstate ss, fileloc loc)
{
  if (sRef_isValid (s))
    {
      /* if (s->defstate == SS_RELDEF) return; */

      if (s->defstate != ss && fileloc_isDefined (loc))
	{
	  s->definfo = alinfo_updateLoc (s->definfo, loc);
	}

      s->defstate = ss;
      sRef_clearDerived (s); 

      if (ss == SS_ALLOCATED)
	{
	  sRef base = sRef_getBaseSafe (s);
	  
	  while (sRef_isValid (base) && sRef_isKnown (base))
	    {
	      if (base->defstate == SS_DEFINED) 
		{ 
		  sRef nb;
		  
		  base->defstate = SS_PDEFINED; 
		  
		  nb = sRef_getBaseSafe (base); 
		  base = nb;
		}
	      else 
		{ 
		  break; 
		}
	    }
	}

          }
}

void sRef_setAllocatedComplete (sRef s, fileloc loc)
{
  sRef_innerAliasSetComplete (sRef_setAllocated, s, loc);
}

static void sRef_setAllocatedShallow (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      if (s->defstate == SS_DEAD || s->defstate == SS_UNDEFINED)
	{
	  s->defstate = SS_ALLOCATED;
	  
	  if (fileloc_isDefined (loc))
	    {
	      s->definfo = alinfo_updateLoc (s->definfo, loc);
	    }
	}
    }
}

void sRef_setAllocatedShallowComplete (sRef s, fileloc loc)
{
  sRef_innerAliasSetComplete (sRef_setAllocatedShallow, s, loc);
}

void sRef_setAllocated (sRef s, fileloc loc)
{
  sRef_setStateAux (s, SS_ALLOCATED, loc);
  }

void sRef_setPartial (sRef s, fileloc loc)
{
  sRef_setStateAux (s, SS_PARTIAL, loc);
  }

void sRef_setShared (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      if (s->aliaskind != AK_SHARED && fileloc_isDefined (loc))
	{
	  s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
	}

      s->aliaskind = AK_SHARED;
      /* don't! sRef_clearDerived (s); */
    }
}

void sRef_setLastReference (sRef s, sRef ref, fileloc loc)
{
  if (sRef_isValid (s))
    {
      s->aliaskind = sRef_getAliasKind (ref);
      s->aliasinfo = alinfo_updateRefLoc (s->aliasinfo, ref, loc);
    }
}

static
void sRef_setNullStateAux (/*@notnull@*/ sRef s, nstate ns, fileloc loc)
{
 s->nullstate = ns;

 if (fileloc_isDefined (loc))
   {
     s->nullinfo = alinfo_updateLoc (s->nullinfo, loc);
   }
}

void sRef_setNotNull (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, NS_NOTNULL, loc);
    }
}

void sRef_setNullState (sRef s, nstate n, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, n, loc);
    }
}

void sRef_setNullTerminatedStateInnerComplete (sRef s, struct _bbufinfo b, /*@unused@*/ fileloc loc) {
   
  switch (b.bufstate) {
     case BB_NULLTERMINATED:
	  sRef_setNullTerminatedState (s);
          sRef_setLen (s, b.len);
          break;
     case BB_POSSIBLYNULLTERMINATED:
          sRef_setPossiblyNullTerminatedState(s);
          break;
     case BB_NOTNULLTERMINATED:
          sRef_setNotNullTerminatedState (s);
          break;
  }
  sRef_setSize (s, b.size);

  /* PL: TO BE DONE : Aliases are not modified right now, have to be similar to
   * setNullStateInnerComplete.
   */
}

void sRef_setNullStateInnerComplete (sRef s, nstate n, fileloc loc)
{
  sRef_setNullState (s, n, loc);

  switch (n)
    {
    case NS_POSNULL:
      sRef_innerAliasSetComplete (sRef_setPosNull, s, loc);
      break;
    case NS_DEFNULL:
      sRef_innerAliasSetComplete (sRef_setDefNull, s, loc);
      break;
    case NS_UNKNOWN:
      sRef_innerAliasSetComplete (sRef_setNullUnknown, s, loc);
      break;
    case NS_NOTNULL:
      sRef_innerAliasSetComplete (sRef_setNotNull, s, loc);
      break;
    case NS_MNOTNULL:
      sRef_innerAliasSetComplete (sRef_setNotNull, s, loc);
      break;
    case NS_RELNULL:
      sRef_innerAliasSetComplete (sRef_setNullUnknown, s, loc);
      break;
    case NS_CONSTNULL:
      sRef_innerAliasSetComplete (sRef_setDefNull, s, loc);
      break;
    case NS_ABSNULL:
      sRef_innerAliasSetComplete (sRef_setNullUnknown, s, loc);
      break;
    case NS_ERROR:
      sRef_innerAliasSetComplete (sRef_setNullErrorLoc, s, loc);
      break;
    }
}

void sRef_setPosNull (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, NS_POSNULL, loc);
    }
}
  
void sRef_setDefNull (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, NS_DEFNULL, loc);
    }
}

void sRef_setNullUnknown (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, NS_UNKNOWN, loc);
    }
}

void sRef_setNullError (sRef s)
{
  if (sRef_isValid (s))
    {
      sRef_setNullStateAux (s, NS_UNKNOWN, fileloc_undefined);
    }
}

void sRef_setNullErrorLoc (sRef s, /*@unused@*/ fileloc loc)
{
  sRef_setNullError (s);
}

void sRef_setOnly (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && s->aliaskind != AK_ONLY)
    {
      s->aliaskind = AK_ONLY;
      s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
          }
}

void sRef_setDependent (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && !sRef_isConst (s) && (s->aliaskind != AK_DEPENDENT))
    {
      s->aliaskind = AK_DEPENDENT;
      s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
          }
}

void sRef_setOwned (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && !sRef_isConst (s) && (s->aliaskind != AK_OWNED))
    {
      s->aliaskind = AK_OWNED;
      s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
          }
}

void sRef_setKept (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && !sRef_isConst (s) && (s->aliaskind != AK_KEPT))
    {
      sRef base = sRef_getBaseSafe (s);  
      
      while (sRef_isValid (base) && sRef_isKnown (base))
	{
	  if (base->defstate == SS_DEFINED) 
	    {
	      base->defstate = SS_PDEFINED; 
	      	      base = sRef_getBaseSafe (base); 
	    }
	  else 
	    {
	      break; 
	    }

	}

      s->aliaskind = AK_KEPT;
      s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
          }
}

static void sRef_setKeptAux (sRef s, fileloc loc)
{
  if (!sRef_isShared (s))
    {
      sRef_setKept (s, loc);
    }
}

static void sRef_setDependentAux (sRef s, fileloc loc)
{
  if (!sRef_isShared (s))
    {
      sRef_setDependent (s, loc);
    }
}

void sRef_setKeptComplete (sRef s, fileloc loc)
{
  sRef_aliasSetComplete (sRef_setKeptAux, s, loc);
}

void sRef_setDependentComplete (sRef s, fileloc loc)
{
  sRef_aliasSetComplete (sRef_setDependentAux, s, loc);
}

void sRef_setFresh (sRef s, fileloc loc)
{
  if (sRef_isValid (s))
    {
      s->aliaskind = AK_FRESH;
      s->aliasinfo = alinfo_updateLoc (s->aliasinfo, loc);
    }
}

void sRef_kill (sRef s, fileloc loc)
{
  DPRINTF (("Kill: %s", sRef_unparseFull (s)));

  if (sRef_isValid (s) && !sRef_isShared (s) && !sRef_isConst (s))
    {
      sRef base = sRef_getBaseSafe (s);  
      
      while (sRef_isValid (base) && sRef_isKnown (base))
	{
	  if (base->defstate == SS_DEFINED) 
	    {
	      base->defstate = SS_PDEFINED; 
	      base = sRef_getBaseSafe (base); 
	    }
	  else 
	    {
	      break; 
	    }

	}
      
      s->aliaskind = s->oaliaskind;
      s->defstate = SS_DEAD;
      s->definfo = alinfo_updateLoc (s->definfo, loc);

      sRef_clearDerived (s);
    }
}

void sRef_maybeKill (sRef s, fileloc loc)
{
        
  if (sRef_isValid (s))
    {
      sRef base = sRef_getBaseSafe (s);  

            
      while (sRef_isValid (base) && sRef_isKnown (base))
	{
	  if (base->defstate == SS_DEFINED || base->defstate == SS_RELDEF)
	    {
	      base->defstate = SS_PDEFINED; 
	      	      base = sRef_getBaseSafe (base); 
	    }
	  else 
	    {
	      	      break; 
	    }
	  
	}
      
      s->aliaskind = s->oaliaskind;
      s->defstate = SS_HOFFA; 
      s->definfo = alinfo_updateLoc (s->definfo, loc);
      sRef_clearDerived (s); 
    }

  }

/*
** just for type checking...
*/

static void sRef_killAux (sRef s, fileloc loc)
{
  if (sRef_isValid (s) && !sRef_isShared (s))
    {
      if (sRef_isUnknownArrayFetch (s))
	{
	  sRef_maybeKill (s, loc);
	}
      else
	{
	  	  sRef_kill (s, loc);
	  	}
    }
}

/*
** kills s and all aliases to s
*/

void sRef_killComplete (sRef s, fileloc loc)
{
  DPRINTF (("Kill complete: %s", sRef_unparseFull (s)));
  sRef_aliasSetComplete (sRef_killAux, s, loc);
}

static bool sRef_equivalent (sRef s1, sRef s2)
{
  return (sRef_compare (s1, s2) == 0);
}

/*
** returns an sRef that will not be free'd on function exit.
*/

/*@only@*/ sRef sRef_saveCopy (sRef s)
{
  sRef ret;

  if (sRef_isValid (s))
    {
      bool old = inFunction;

      /*
      ** Exit the function scope, so this sRef is not
      ** stored in the deallocation table.
      */
      
      inFunction = FALSE;
      DPRINTF (("Copying sref: %s", sRef_unparseFull(s)));
      ret = sRef_copy (s);
      DPRINTF (("Copying ===>: %s", sRef_unparseFull(ret)));
      inFunction = old;
    }
  else
    {
      ret = sRef_undefined;
    }

  /*@-dependenttrans@*/ 
  return ret;
  /*@=dependenttrans@*/ 
}

sRef sRef_copy (sRef s)
{
  if (sRef_isKindSpecial (s))
    {
      /*@-retalias@*/
      return s; /* don't copy specials */
      /*@=retalias@*/
    }
  
  if (sRef_isValid (s))
    {
      sRef t = sRef_alloc ();

      t->kind = s->kind;
      t->safe = s->safe;
      t->modified = s->modified;
      t->type = s->type;

            t->info = sinfo_copy (s);
      
      t->defstate = s->defstate;

      t->nullstate = s->nullstate;
 
      /* start modifications */
      t->bufinfo.bufstate = s->bufinfo.bufstate;
      t->bufinfo.len = s->bufinfo.len;
      t->bufinfo.size = s->bufinfo.size;
      /* end modifications */

      t->aliaskind = s->aliaskind;
      t->oaliaskind = s->oaliaskind;

      t->expkind = s->expkind;
      t->oexpkind = s->oexpkind;

      t->aliasinfo = alinfo_copy (s->aliasinfo);
      t->definfo = alinfo_copy (s->definfo);
      t->nullinfo = alinfo_copy (s->nullinfo);
      t->expinfo = alinfo_copy (s->expinfo);

      t->deriv = sRefSet_newDeepCopy (s->deriv);
      
      return t;
    }
  else
    {
      return sRef_undefined;
    }
}

/*@notfunction@*/
# define PREDTEST(func,s) \
   do { if (sRef_isInvalid (s)) { return FALSE; } \
        else { if (sRef_isConj (s)) \
                  { return (func (sRef_getConjA (s)) \
		            || func (sRef_getConjB (s))); }}} while (FALSE);

bool sRef_isAddress (sRef s)
{
  PREDTEST (sRef_isAddress, s);
  return (s->kind == SK_ADR);
}
	  
/*
** pretty weak... maybe a flag should control this.
*/

bool sRef_isThroughArrayFetch (sRef s)
{
  if (sRef_isValid (s))
    {
      sRef tref = s;

      do 
	{
	  sRef lt;

	  if (sRef_isArrayFetch (tref)) 
	    {
	      	      return TRUE;
	    }
	  
	  lt = sRef_getBase (tref);
	  tref = lt;
	} while (sRef_isValid (tref));
    } 

  return FALSE;
}

bool sRef_isArrayFetch (sRef s)
{
  PREDTEST (sRef_isArrayFetch, s);
  return (s->kind == SK_ARRAYFETCH);
}

bool sRef_isMacroParamRef (sRef s)
{
  if (context_inMacro () && sRef_isCvar (s))
    {
      uentry ue = sRef_getUentry (s);
      cstring pname = makeParam (uentry_rawName (ue));
      uentry mac = usymtab_lookupSafe (pname);

      cstring_free (pname);
      return (uentry_isValid (mac));
    }

  return FALSE;
}
      
bool sRef_isCvar (sRef s) 
{
  PREDTEST (sRef_isCvar, s);
  return (s->kind == SK_CVAR);
}

bool sRef_isConst (sRef s) 
{
  PREDTEST (sRef_isConst, s);
  return (s->kind == SK_CONST);
}

bool sRef_isObject (sRef s) 
{
  PREDTEST (sRef_isObject, s);
  return (s->kind == SK_OBJECT);
}

bool sRef_isExternal (sRef s) 
{
  PREDTEST (sRef_isExternal, s);
  return (s->kind == SK_EXTERNAL);
}

static bool sRef_isDerived (sRef s) 
{
  PREDTEST (sRef_isDerived, s);
  return (s->kind == SK_DERIVED);
}

bool sRef_isField (sRef s)
{
  PREDTEST (sRef_isField, s);
  return (s->kind == SK_FIELD);
}

static bool sRef_isIndex (sRef s)
{
  PREDTEST (sRef_isIndex, s);
  return (s->kind == SK_ARRAYFETCH);
}

bool sRef_isAnyParam (sRef s)
{
  PREDTEST (sRef_isAnyParam, s);
  return (s->kind == SK_PARAM);  
}

bool sRef_isParam (sRef s)
{
  PREDTEST (sRef_isParam, s);
  return (s->kind == SK_PARAM);
}

bool sRef_isDirectParam (sRef s)
{
  PREDTEST (sRef_isDirectParam, s);

  return ((s->kind == SK_CVAR) &&
	  (s->info->cvar->lexlevel == functionScope) &&
	  (context_inFunction () && 
	   (s->info->cvar->index <= uentryList_size (context_getParams ()))));
}

bool sRef_isPointer (sRef s)
{
  PREDTEST (sRef_isPointer, s);
  return (s->kind == SK_PTR);
}

/*
** returns true if storage referenced by s is visible
*/

bool sRef_isReference (sRef s)
{
  PREDTEST (sRef_isReference, s);

  return (sRef_isPointer (s) || sRef_isIndex (s) || sRef_isGlobal (s)
	  || (sRef_isField (s) && (sRef_isReference (s->info->field->rec))));
}

bool sRef_isIReference (sRef s)
{
  return (sRef_isPointer (s) || sRef_isAddress (s) || sRef_isIndex (s)
	  || sRef_isField (s) || sRef_isArrayFetch (s));
}

bool sRef_isGlobal (sRef s)
{
  return (sRef_isCvar (s) && (s->info->cvar->lexlevel <= fileScope));
}

bool sRef_isRealGlobal (sRef s)
{
  return (sRef_isCvar (s) && (s->info->cvar->lexlevel == globScope));
}

bool sRef_isFileStatic (sRef s)
{
  return (sRef_isCvar (s) && (s->info->cvar->lexlevel == fileScope));
}

bool sRef_isAliasCheckedGlobal (sRef s)
{
  if (sRef_isGlobal (s))
    {
      uentry ue = sRef_getUentry (s);

      return context_checkAliasGlob (ue);
    }
  else
    {
      return FALSE;
    }
}

void sRef_free (/*@only@*/ sRef s)
{
  if (s != sRef_undefined && s->kind != SK_SPECIAL)
    {
      alinfo_free (s->expinfo);
      alinfo_free (s->aliasinfo);
      alinfo_free (s->definfo);
      alinfo_free (s->nullinfo);
      
      sRefSet_free (s->deriv);
      s->deriv = sRefSet_undefined;
      sinfo_free (s);
      
      sfree (s); 
    }
}

void sRef_setType (sRef s, ctype t)
{
  if (sRef_isValid (s))
    {
      s->type = t;
    }
}

void sRef_setTypeFull (sRef s, ctype t)
{
  if (sRef_isValid (s))
    {
      s->type = t;

      sRefSet_allElements (s->deriv, current)
	{
	  sRef_setTypeFull (current, ctype_unknown);
	} end_sRefSet_allElements ;
    }
}

/*@exposed@*/ sRef
  sRef_buildField (sRef rec, /*@dependent@*/ cstring f)
{
  return (sRef_buildNCField (rec, f)); 
}

static /*@exposed@*/ sRef
sRef_findDerivedField (/*@notnull@*/ sRef rec, cstring f)
{
  sRefSet_allElements (rec->deriv, sr)
    {
      if (sRef_isValid (sr))
	{
	  if (sr->kind == SK_FIELD && cstring_equal (sr->info->field->field, f))
	    {
	      return sr;
	    }
	}
    } end_sRefSet_allElements;

  return sRef_undefined;
}

/*@dependent@*/ /*@observer@*/ sRefSet
  sRef_derivedFields (sRef rec)
{
  if (sRef_isValid (rec))
    {
      sRefSet ret;
      ret = rec->deriv;
      return (ret);
    }
  else
    {
      return (sRefSet_undefined);
    }
}

static /*@exposed@*/ sRef
  sRef_findDerivedPointer (sRef s)
{
  if (sRef_isValid (s))
    {
      sRefSet_realElements (s->deriv, sr)
	{
	  if (sRef_isValid (sr) && sr->kind == SK_PTR)
	    {
	      return sr;
	    }
	} end_sRefSet_realElements;
    }

  return sRef_undefined;
}

bool
sRef_isUnknownArrayFetch (sRef s)
{
  return (sRef_isValid (s) 
	  && s->kind == SK_ARRAYFETCH
	  && !s->info->arrayfetch->indknown);
}

static /*@exposed@*/ sRef
sRef_findDerivedArrayFetch (/*@notnull@*/ sRef s, bool isknown, int idx, bool dead)
{
  
  if (isknown) 
    {
      sRefSet_realElements (s->deriv, sr)
	{
	  if (sRef_isValid (sr)
	      && sr->kind == SK_ARRAYFETCH
	      && sr->info->arrayfetch->indknown
	      && (sr->info->arrayfetch->ind == idx))
	    {
	      return sr;
	    }
	} end_sRefSet_realElements;
    }
  else
    {
      sRefSet_realElements (s->deriv, sr)
	{
	  if (sRef_isValid (sr)
	      && sr->kind == SK_ARRAYFETCH
	      && (!sr->info->arrayfetch->indknown
		  || (sr->info->arrayfetch->indknown && 
		      sr->info->arrayfetch->ind == 0)))
	    {
	      if (sRef_isDead (sr) || sRef_isKept (sr))
		{
		  if (dead || context_getFlag (FLG_STRICTUSERELEASED))
		    {
		      return sr;
		    }
		}
	      else
		{
		  return sr;
		}
	    }
	} end_sRefSet_realElements;
    }

  return sRef_undefined;
}

static /*@exposed@*/ sRef 
sRef_buildNCField (/*@exposed@*/ sRef rec, /*@exposed@*/ cstring f)
{
  sRef s;

  if (sRef_isInvalid (rec))
    {
      return sRef_undefined;
    }
      
  /*
  ** check if the field already has been referenced 
  */

  s = sRef_findDerivedField (rec, f);
  
  if (sRef_isValid (s))
    {
            return s;
    }
  else
    {
      ctype ct = ctype_realType (rec->type);

      s = sRef_new ();      
      s->kind = SK_FIELD;
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->field = (fldinfo) dmalloc (sizeof (*s->info->field));
      s->info->field->rec = rec;
      s->info->field->field = f; /* doesn't copy f */
      
      
      if (ctype_isKnown (ct) && ctype_isSU (ct))
	{
	  uentry ue = uentryList_lookupField (ctype_getFields (ct), f);
	
	  if (!uentry_isUndefined (ue))
	    {
	      s->type = uentry_getType (ue);

	      if (ctype_isMutable (s->type)
		  && rec->aliaskind != AK_STACK 
		  && !alkind_isStatic (rec->aliaskind))
		{
		  s->aliaskind = rec->aliaskind;
		}
	      else
		{
		  s->aliaskind = AK_UNKNOWN;
		}

	      if (sRef_isStateDefined (rec) || sRef_isStateUnknown (rec) 
		  || sRef_isPdefined (rec))
		{
		  		  sRef_setStateFromUentry (s, ue);
		  		}
	      else
		{
		  sRef_setPartsFromUentry (s, ue);
		  		}

	      s->oaliaskind = s->aliaskind;
	      s->oexpkind = s->expkind;
	    }
	  else
	    {
	      /*
		Never report this as an error.  It can happen whenever there
		is casting involved.

	      if (report)
		{
		  llcontbug (message ("buildNCField --- no field %s: %q / %s",
				      f, sRef_unparse (s), ctype_unparse (ct)));
		}
		*/

	      return sRef_undefined;
	    }
	}
      
      if (rec->defstate == SS_DEFINED 
	  && (s->defstate == SS_UNDEFINED || s->defstate == SS_UNKNOWN))
	{
	  s->defstate = SS_DEFINED;
	}
      else if (rec->defstate == SS_PARTIAL)
	{
	  s->defstate = SS_PARTIAL;
	}
      else if (rec->defstate == SS_ALLOCATED) 
	{
	  if (ctype_isStackAllocated (ct) && ctype_isStackAllocated (s->type))
	    {
	      s->defstate = SS_ALLOCATED;
	    }
	  else
	    {
	      s->defstate = SS_UNDEFINED;
	    }
	}
      else if (s->defstate == SS_UNKNOWN)
	{
	  s->defstate = rec->defstate;
	}
      else
	{
	  ; /* no change */
	}

      if (s->defstate == SS_UNDEFINED)
	{
	  ctype rt = ctype_realType (s->type);
	  
	  if (ctype_isArray (rt) || ctype_isSU (rt))
	    {
	      	      s->defstate = SS_ALLOCATED;
	    }
	}

      sRef_addDeriv (rec, s);

      if (ctype_isInt (s->type) && cstring_equal (f, REFSNAME))
	{
	  s->aliaskind = AK_REFS;
	  s->oaliaskind = AK_REFS;
	}

            return s;
    }
}

bool
sRef_isStackAllocated (sRef s)
{
  return (sRef_isValid(s) 
	  && s->defstate == SS_ALLOCATED && ctype_isStackAllocated (s->type));
}
	  
static
void sRef_setArrayFetchState (/*@notnull@*/ /*@exposed@*/ sRef s, 
			      /*@notnull@*/ /*@exposed@*/ sRef arr)
{
  if (ctype_isRealAP (arr->type))
    {
      s->type = ctype_baseArrayPtr (arr->type);
    }

  /* a hack, methinks... makeArrayFetch (&a[0]) ==> a[] */
  if (sRef_isAddress (arr)) 
    {
      sRef t = arr->info->ref;
      
      if (sRef_isArrayFetch (t))
	{
	  s->info->arrayfetch->arr = t->info->arrayfetch->arr;
	}
    }
  else if (ctype_isRealPointer (arr->type))
    {
      sRef sp = sRef_findDerivedPointer (arr);

      
      if (sRef_isValid (sp))
	{
	  
	  if (ctype_isMutable (s->type))
	    {
	      sRef_setExKind (s, sRef_getExKind (sp), fileloc_undefined);

	      	      
	      s->aliaskind = sp->aliaskind;
	    }

	  s->defstate = sp->defstate;

	  if (s->defstate == SS_DEFINED) 
	    {
	      if (!context_getFlag (FLG_STRICTDESTROY))
		{
		  s->defstate = SS_PARTIAL;
		}
	    }

	  s->nullstate = sp->nullstate;
	}
      else
	{
	  if (arr->defstate == SS_UNDEFINED)
	    {
	      s->defstate = SS_UNUSEABLE;
	    }
	  else if ((arr->defstate == SS_ALLOCATED) && !ctype_isSU (s->type))
	    {
	      s->defstate = SS_UNDEFINED;
	    }
	  else
	    {
	      if (!context_getFlag (FLG_STRICTDESTROY))
		{
		  s->defstate = SS_PARTIAL;
		}
	      else
		{
		  s->defstate = SS_DEFINED;
		}

	      /*
	      ** Very weak checking for array elements.
	      ** Was:
	      **     s->defstate = arr->defstate;
	      */
	    }

	  sRef_setExKind (s, sRef_getExKind (arr), g_currentloc);

	  if (arr->aliaskind == AK_LOCAL || arr->aliaskind == AK_FRESH)
	    {
	      s->aliaskind = AK_LOCAL;
	    }
	  else
	    {
	      s->aliaskind = AK_UNKNOWN;
	    }
	  
	  sRef_setTypeState (s);
	}
    }
  else
    {
      if (arr->defstate == SS_DEFINED)
	{
	  /*
	  ** Very weak checking for array elements.
	  ** Was:
	  **     s->defstate = arr->defstate;
	  */

	  if (context_getFlag (FLG_STRICTDESTROY))
	    {
	      s->defstate = SS_DEFINED;
	    }
	  else
	    {
	      s->defstate = SS_PARTIAL;
	    }
	}
      else if (arr->defstate == SS_ALLOCATED)
	{
	  if (ctype_isRealArray (s->type))
	    {
	      s->defstate = SS_ALLOCATED;
	    }
	  else 
	    {
	      if (!s->info->arrayfetch->indknown)
		{
		  /*
		  ** is index is unknown, elements is defined or 
		  ** allocated is any element is!
		  */
		  
		  s->defstate = SS_UNDEFINED;
		  
		  sRefSet_allElements (arr->deriv, sr)
		    {
		      if (sRef_isValid (sr))
			{
			  if (sr->defstate == SS_ALLOCATED)
			    {
			      s->defstate = SS_ALLOCATED;
			    }
			  else 
			    {
			      if (sr->defstate == SS_DEFINED)
				{
				  if (context_getFlag (FLG_STRICTDESTROY))
				    {
				      s->defstate = SS_DEFINED;
				    }
				  else
				    {
				      s->defstate = SS_PARTIAL;
				    }

				  break;
				}
			    }
			}
		    } end_sRefSet_allElements;
		  
		  		}
	      else
		{
		  s->defstate = SS_UNDEFINED;
		}
	    }
	}
      else
	{
	  s->defstate = arr->defstate;
	}
      
      
      /*
      ** kludgey way to guess where aliaskind applies
      */
      
      if (ctype_isMutable (s->type) 
	  && !ctype_isPointer (arr->type) 
	  && !alkind_isStatic (arr->aliaskind)
	  && !alkind_isStack (arr->aliaskind)) /* evs - 2000-06-20: don't pass stack allocation to members */
	{
	  s->aliaskind = arr->aliaskind;
	}
      else
	{
	  s->aliaskind = AK_UNKNOWN;
	}
    
      sRef_setTypeState (s);
    }

  if (sRef_isObserver (arr)) 
    {
      s->expkind = XO_OBSERVER;
    }
}  

/*@exposed@*/ sRef sRef_buildArrayFetch (/*@exposed@*/ sRef arr)
{
  sRef s;

  if (!sRef_isValid (arr)) {
    /*@-nullret@*/ return arr /*@=nullret@*/;
  }

  if (ctype_isRealPointer (arr->type))
    {
      (void) sRef_buildPointer (arr); /* do this to define arr! */
    }
  
  s = sRef_findDerivedArrayFetch (arr, FALSE, 0, FALSE);
  
  if (sRef_isValid (s))
    {
      sRef_setExKind (s, sRef_getExKind (arr), g_currentloc);
      return s;
    }
  else
    {
      s = sRef_new ();

      s->kind = SK_ARRAYFETCH;
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->arrayfetch = (ainfo) dmalloc (sizeof (*s->info->arrayfetch));
      s->info->arrayfetch->indknown = FALSE;
      s->info->arrayfetch->ind = 0;
      s->info->arrayfetch->arr = arr;
      sRef_setArrayFetchState (s, arr);
      s->oaliaskind = s->aliaskind;
      s->oexpkind = s->expkind;

      if (!context_inProtectVars ())
	{
	  sRef_addDeriv (arr, s);
	}
      
      return (s);
    }
}

/*@exposed@*/ sRef
  sRef_buildArrayFetchKnown (/*@exposed@*/ sRef arr, int i)
{
  sRef s;

  if (!sRef_isValid (arr)) {
    /*@-nullret@*/ return arr /*@=nullret@*/;
  }

  if (ctype_isRealPointer (arr->type))
    {
       (void) sRef_buildPointer (arr); /* do this to define arr! */
    }

  s = sRef_findDerivedArrayFetch (arr, TRUE, i, FALSE);
      
  if (sRef_isValid (s))
    {
      sRef_setExKind (s, sRef_getExKind (arr), g_currentloc);      
      return s;
    }
  else
    {
      s = sRef_new ();
      
      s->kind = SK_ARRAYFETCH;
      s->info = (sinfo) dmalloc (sizeof (*s->info));
      s->info->arrayfetch = (ainfo) dmalloc (sizeof (*s->info->arrayfetch));
      s->info->arrayfetch->arr = arr;
      s->info->arrayfetch->indknown = TRUE;
      s->info->arrayfetch->ind = i;
      
      sRef_setArrayFetchState (s, arr);
      
      s->oaliaskind = s->aliaskind;
      s->oexpkind = s->expkind;
      sRef_addDeriv (arr, s);

      return (s);
    }
}

/*
** sets everything except for defstate
*/

static void
sRef_setPartsFromUentry (sRef s, uentry ue)
{
    
  llassert (sRef_isValid (s));

  s->aliaskind = alkind_derive (s->aliaskind, uentry_getAliasKind (ue));
  s->oaliaskind = s->aliaskind;

  if (s->expkind == XO_UNKNOWN)
    {
      s->expkind = uentry_getExpKind (ue);
    }

  s->oexpkind = s->expkind;

  if (s->nullstate == NS_UNKNOWN)
    {
      s->nullstate = sRef_getNullState (uentry_getSref (ue));
    }

  if (s->aliaskind == AK_IMPONLY 
      && (sRef_isExposed (s) || sRef_isObserver (s)))
    {
      s->oaliaskind = s->aliaskind = AK_IMPDEPENDENT;
    }

}

static void
sRef_setStateFromAbstractUentry (sRef s, uentry ue)
{
  llassert (sRef_isValid (s));
  
  sRef_setPartsFromUentry (s, ue);

  s->aliaskind = alkind_derive (s->aliaskind, uentry_getAliasKind (ue));
  s->oaliaskind = s->aliaskind;

  if (s->expkind == XO_UNKNOWN)
    {
      s->expkind = uentry_getExpKind (ue);
    }

  s->oexpkind = s->expkind;
}

void
sRef_setStateFromUentry (sRef s, uentry ue)
{
  sstate defstate;

  llassert (sRef_isValid (s));
  
  sRef_setPartsFromUentry (s, ue);

  defstate = uentry_getDefState (ue);

  if (sstate_isKnown (defstate))
    {
      s->defstate = defstate;
    }
  else
    {
      ;
    }
}

/*@exposed@*/ sRef
  sRef_buildPointer (/*@exposed@*/ sRef t)
{
  DPRINTF (("build pointer: %s", sRef_unparse (t)));

  if (sRef_isInvalid (t)) return sRef_undefined;

  if (sRef_isAddress (t))
    {
      DPRINTF (("Return ref: %s", sRef_unparse (t->info->ref)));
      return (t->info->ref);
    }
  else
    {
      sRef s = sRef_findDerivedPointer (t);

      DPRINTF (("find derived: %s", sRef_unparse (s)));

      if (sRef_isValid (s))
	{
	  
	  sRef_setExKind (s, sRef_getExKind (t), g_currentloc);
	  s->oaliaskind = s->aliaskind;
	  s->oexpkind = s->expkind;

	  return s;
	}
      else
	{
	  s = sRef_constructPointerAux (t);
	  
	  DPRINTF (("construct: %s", sRef_unparse (s)));

	  if (sRef_isValid (s))
	    {
	      sRef_addDeriv (t, s);

	      s->oaliaskind = s->aliaskind;
	      s->oexpkind = s->expkind;
	    }
	  
	  return s;
	}
    }
}

/*@exposed@*/ sRef
sRef_constructPointer (sRef t)
   /*@modifies t@*/
{
  return sRef_buildPointer (t);
}

static /*@exposed@*/ sRef sRef_constructDerefAux (sRef t, bool isdead)
{
  if (sRef_isValid (t))
    {
      sRef s;
      
      /*
      ** if there is a derived t[?], return that.  Otherwise, *t.
      */
      
            
      s = sRef_findDerivedArrayFetch (t, FALSE, 0, isdead);
      
      if (sRef_isValid (s))
	{
	  	  return s;
	}
      else
	{
	  sRef ret = sRef_constructPointer (t);

	  /*
	  ** This is necessary to prevent infinite depth
	  ** in checking complete destruction.  
	  */

	  
	  if (isdead)
	    {
	      /* ret->defstate = SS_UNKNOWN;  */
	      return ret; 
	    }
	  else
	    {
	      return ret;
	    }
	}
    }
  else
    {
      return sRef_undefined;
    }
}

sRef sRef_constructDeref (sRef t)
{
  return sRef_constructDerefAux (t, FALSE);
}

sRef sRef_constructDeadDeref (sRef t)
{
  return sRef_constructDerefAux (t, TRUE);
}

static sRef
sRef_constructPointerAux (/*@notnull@*/ /*@exposed@*/ sRef t)
{
  sRef s = sRef_new ();
  ctype rt = t->type;
  ctype st;
  
  s->kind = SK_PTR;
  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->ref = t;
  
  if (ctype_isRealAP (rt))
    {
      s->type = ctype_baseArrayPtr (rt);
    }
  
  st = ctype_realType (s->type);
  
    
  if (t->defstate == SS_UNDEFINED)
    {
      s->defstate = SS_UNUSEABLE;
    }
  else if ((t->defstate == SS_ALLOCATED) && !ctype_isSU (st))
    {
      s->defstate = SS_UNDEFINED;
    }
  else
    {
      s->defstate = t->defstate;
    }
  
  if (t->aliaskind == AK_LOCAL || t->aliaskind == AK_FRESH)
    {
      s->aliaskind = AK_LOCAL;
    }
  else
    {
      s->aliaskind = AK_UNKNOWN;
    }
  
  sRef_setExKind (s, sRef_getExKind (t), fileloc_undefined);
  sRef_setTypeState (s);
  
  s->oaliaskind = s->aliaskind;
  s->oexpkind = s->expkind;

  return s;
}

bool sRef_hasDerived (sRef s)
{
  return (sRef_isValid (s) && !sRefSet_isEmpty (s->deriv));
}

void
sRef_clearDerived (sRef s)
{
  if (sRef_isValid (s))
    {
            sRefSet_clear (s->deriv); 
    }
}

void
sRef_clearDerivedComplete (sRef s)
{
  
  if (sRef_isValid (s))
    {
      sRef base = sRef_getBaseSafe (s);

      while (sRef_isValid (base))
	{
	  sRefSet_clear (base->deriv); 
	  base = sRef_getBaseSafe (base);
	}

      sRefSet_clear (s->deriv); 
    }
}

/*@exposed@*/ sRef
sRef_makePointer (sRef s)
   /*@modifies s@*/
{
  sRef res = sRef_buildPointer (s); 

  DPRINTF (("Res: %s", sRef_unparse (res)));
  return res;
}

/*
** &a[] => a (this is for out params)
*/

/*@exposed@*/ sRef
sRef_makeAnyArrayFetch (/*@exposed@*/ sRef arr)
{
  
  if (sRef_isAddress (arr))
    {
            return (arr->info->ref);
    }
  else
    {
      return (sRef_buildArrayFetch (arr));
    }
}

/*@exposed@*/ sRef
sRef_makeArrayFetch (sRef arr)
{
  return (sRef_buildArrayFetch (arr));
}

/*@exposed@*/ sRef
sRef_makeArrayFetchKnown (sRef arr, int i)
{
  return (sRef_buildArrayFetchKnown (arr, i));
}

/*@exposed@*/ sRef
sRef_makeField (sRef rec, /*@dependent@*/ cstring f)
{
  sRef ret;
  ret = sRef_buildField (rec, f);
  return ret;
}

/*@exposed@*/ sRef
sRef_makeNCField (sRef rec, /*@dependent@*/ cstring f)
{
    return (sRef_buildNCField (rec, f));
}

/*@only@*/ cstring
sRef_unparseKindName (sRef s)
{
  cstring result;

  if (s == sRef_undefined) return cstring_makeLiteral ("<invalid>");

  s = sRef_fixConj (s);

  switch (s->kind)
    {
    case SK_CVAR: 
      if (sRef_isLocalVar (s)) 
	{
	  result = cstring_makeLiteral ("Variable");
	}
      else
	{
	  result = cstring_makeLiteral ("Undef global");
	}
      break;
    case SK_PARAM:
      result = cstring_makeLiteral ("Out parameter");
      break;
    case SK_ARRAYFETCH:
      if (sRef_isAnyParam (s->info->arrayfetch->arr)) 
	{
	  result = cstring_makeLiteral ("Out parameter");
	}
      else if (sRef_isIndexKnown (s))
	{
	  result = cstring_makeLiteral ("Array element");
	}
      else
	{
	  result = cstring_makeLiteral ("Value");
	}
      break;
    case SK_PTR:
      if (sRef_isAnyParam (s->info->ref)) 
	{
	  result = cstring_makeLiteral ("Out parameter");
	}
      else
	{
	  result = cstring_makeLiteral ("Value");
	}
      break;
    case SK_ADR:
      result = cstring_makeLiteral ("Value");
      break;
    case SK_FIELD:
      result = cstring_makeLiteral ("Field");
      break;
    case SK_OBJECT:
      result = cstring_makeLiteral ("Object");
      break;
    case SK_UNCONSTRAINED:
      result = cstring_makeLiteral ("<anything>");
      break;
    case SK_RESULT:
    case SK_SPECIAL:
    case SK_UNKNOWN:
    case SK_EXTERNAL:
    case SK_DERIVED:
    case SK_CONST:
    case SK_TYPE:
      result = cstring_makeLiteral ("<unknown>");
      break;
    case SK_CONJ:
      result = cstring_makeLiteral ("<conj>");
      break;
    case SK_NEW:
      result = cstring_makeLiteral ("Storage");
      break;
    }
  
  return result;
}

/*@only@*/ cstring
sRef_unparseKindNamePlain (sRef s)
{
  cstring result;

  if (s == sRef_undefined) return cstring_makeLiteral ("<invalid>");

  s = sRef_fixConj (s);

  switch (s->kind)
    {
    case SK_CVAR: 
      if (sRef_isLocalVar (s)) 
	{
	  result = cstring_makeLiteral ("Variable");
	}
      else 
	{
	  result = cstring_makeLiteral ("Global");
	}
      break;
    case SK_PARAM:
      result = cstring_makeLiteral ("Parameter");
      break;
    case SK_ARRAYFETCH:
      if (sRef_isAnyParam (s->info->arrayfetch->arr)) 
	{
	  result = cstring_makeLiteral ("Parameter");
	}
      else if (sRef_isIndexKnown (s))
	{
	  result = cstring_makeLiteral ("Array element");
	}
      else 
	{
	  result = cstring_makeLiteral ("Value");
	}
      break;
    case SK_PTR:
      if (sRef_isAnyParam (s->info->ref))
	{
	  result = cstring_makeLiteral ("Parameter");
	}
      else
	{
	  result = cstring_makeLiteral ("Value");
	}
      break;
    case SK_ADR:
      result = cstring_makeLiteral ("Value");
      break;
    case SK_FIELD:
      result = cstring_makeLiteral ("Field");
      break;
    case SK_OBJECT:
      result = cstring_makeLiteral ("Object");
      break;
    case SK_NEW:
      result = cstring_makeLiteral ("Storage");
      break;
    case SK_UNCONSTRAINED:
      result = cstring_makeLiteral ("<anything>");
      break;
    case SK_RESULT:
    case SK_TYPE:
    case SK_CONST:
    case SK_EXTERNAL:
    case SK_DERIVED:
    case SK_UNKNOWN:
    case SK_SPECIAL:
      result = cstring_makeLiteral ("<unknown>");
      break;
    case SK_CONJ:
      result = cstring_makeLiteral ("<conj>");
      break;
    }
  
  return result;
}

/*
** s1 <- s2
*/

void
sRef_copyState (sRef s1, sRef s2)
{
  if (sRef_isValid (s1) && sRef_isValid (s2))
    {
      s1->defstate = s2->defstate;
      
      s1->nullstate = s2->nullstate;
      s1->nullinfo = alinfo_update (s1->nullinfo, s2->nullinfo);
      
      /* start modifications */
      s1->bufinfo.bufstate = s2->bufinfo.bufstate;
      s1->bufinfo.len = s2->bufinfo.len;
      s1->bufinfo.size = s2->bufinfo.size;
      /* end modifications */

      s1->aliaskind = s2->aliaskind;
      s1->aliasinfo = alinfo_update (s1->aliasinfo, s2->aliasinfo);

      s1->expkind = s2->expkind;
      s1->expinfo = alinfo_update (s1->expinfo, s2->expinfo);

      s1->safe = s2->safe;
          }
}

sRef
sRef_makeNew (ctype ct, sRef t, cstring name)
{
  sRef s = sRef_new ();

  s->kind = SK_NEW;
  s->type = ct;

  llassert (sRef_isValid (t));
  s->defstate = t->defstate;

  s->aliaskind = t->aliaskind;
  s->oaliaskind = s->aliaskind;

  s->nullstate = t->nullstate;

  s->expkind = t->expkind;
  s->oexpkind = s->expkind;

  s->info = (sinfo) dmalloc (sizeof (*s->info));
  s->info->fname = name;

  /* start modifications */
  s->bufinfo.bufstate = t->bufinfo.bufstate;
  /* end modifications */

    return s;
}

sRef
sRef_makeType (ctype ct)
{
  sRef s = sRef_new ();
  
  s->kind = SK_TYPE;
  s->type = ct;

  s->defstate = SS_UNKNOWN; 
  s->aliaskind = AK_UNKNOWN;
  s->nullstate = NS_UNKNOWN;
  /* start modification */
  s->bufinfo.bufstate = BB_NOTNULLTERMINATED;
  /* end modification */

    
  if (ctype_isUA (ct))
    {
      typeId uid = ctype_typeId (ct);
      uentry ue = usymtab_getTypeEntrySafe (uid);

      if (uentry_isValid (ue))
	{
	  sRef_mergeStateQuiet (s, uentry_getSref (ue));
	}
    }

    s->oaliaskind = s->aliaskind;
  s->oexpkind = s->expkind;

  return s;
}

sRef
sRef_makeConst (ctype ct)
{
  sRef s = sRef_new ();
  
  s->kind = SK_CONST;
  s->type = ct;

  s->defstate = SS_UNKNOWN;
  s->aliaskind = AK_UNKNOWN;
  s->nullstate = NS_UNKNOWN;
  /* start modification */
  s->bufinfo.bufstate = BB_NULLTERMINATED;
  /* end modification */

  
  if (ctype_isUA (ct))
    {
      typeId uid = ctype_typeId (ct);
      uentry te = usymtab_getTypeEntrySafe (uid);
      
      if (uentry_isValid (te))
	{
	  sRef_mergeStateQuiet (s, uentry_getSref (te));
	}
    }

  
  s->oaliaskind = s->aliaskind;
  s->oexpkind = s->expkind;

  return s;
}

bool sRef_hasName (sRef s)
{
  if (sRef_isInvalid (s))
    {
      return (FALSE);
    }

  switch (s->kind)
    {
    case SK_CVAR:
      {
	uentry u = usymtab_getRefQuiet (s->info->cvar->lexlevel,
					 s->info->cvar->index);
	return (uentry_hasName (u));
      }
    case SK_PARAM:
      {
	uentry u = uentryList_getN (context_getParams (), 
				    s->info->paramno);

	return (uentry_hasName (u));
      }
    default:
      return TRUE;
    }
}

bool sRef_sameObject (sRef s1, sRef s2)
{
  return sRef_sameName(s1, s2);
}
bool
sRef_sameName (sRef s1, sRef s2)
{
  if (sRef_isInvalid (s1))
    {
      return sRef_isInvalid (s2);
    }

  if (sRef_isInvalid (s2))
    {
      return (FALSE);
    }

  switch (s1->kind)
    {
    case SK_CVAR:
      if (s2->kind == SK_CVAR)
	{
	  return (s1->info->cvar->lexlevel == s2->info->cvar->lexlevel
		  && s1->info->cvar->index == s2->info->cvar->index);
	}
      else if (s2->kind == SK_PARAM)
	{
	  if (context_inFunctionLike ())
	    {
	      uentry u1 = usymtab_getRefQuiet (s1->info->cvar->lexlevel,
					       s1->info->cvar->index);
	      uentry u2 = uentryList_getN (context_getParams (), 
					   s2->info->paramno);
	  
	      return (cstring_equalFree (uentry_getName (u1),
					 uentry_getName (u2)));
	    }
	  else 
	    {
	      return FALSE;
	    }
	}
      else
	{
	  return FALSE;
	}
    case SK_PARAM:
      {
	if (s2->kind == SK_PARAM)
	  {
	    return (s1->info->paramno == s2->info->paramno);
	  }
	else if (s2->kind == SK_CVAR)
	  {
	    if (context_inFunctionLike ())
	      {
		uentry u1 = uentryList_getN (context_getParams (), 
					     s1->info->paramno);
		uentry u2 = usymtab_getRefQuiet (s2->info->cvar->lexlevel,
						 s2->info->cvar->index);

		
		return (cstring_equalFree (uentry_getName (u1),
					   uentry_getName (u2)));
	      }
	    else 
	      {
		return FALSE;
	      }
	  }
	else
	  {
	    return FALSE;
	  }
      }

    case SK_UNCONSTRAINED:
      return FALSE;

    case SK_ARRAYFETCH:
      if (s2->kind == SK_ARRAYFETCH)
	{
	  if (bool_equal (s1->info->arrayfetch->indknown,
			  s2->info->arrayfetch->indknown))
	    {
	      if (!s1->info->arrayfetch->indknown 
		  || (s1->info->arrayfetch->ind == s2->info->arrayfetch->ind))
		{
		  return sRef_sameName (s1->info->arrayfetch->arr,
					s2->info->arrayfetch->arr);
		}
	    }
	}

      return FALSE;
    case SK_FIELD:
      if (s2->kind == SK_FIELD)
	{
	  if (cstring_equal (s1->info->field->field,
			     s2->info->field->field))
	    {
	      return sRef_sameName (s1->info->field->rec,
				    s2->info->field->rec);
	    }

	}
      return FALSE;
    case SK_PTR:
    case SK_ADR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      if (s2->kind == s1->kind)
	{
	  return sRef_sameName (s1->info->ref,
				s2->info->ref);
	}

      return FALSE;
    case SK_OBJECT:
      return FALSE;
    case SK_CONJ:
      return sRef_sameName (sRef_getConjA (s1), s2);
    case SK_NEW:
      return FALSE;
    case SK_UNKNOWN:
      return (s2->kind == SK_UNKNOWN);
    case SK_TYPE:
    case SK_CONST:
      if (s2->kind == s1->kind)
	{
	  return (ctype_equal (s1->type, s2->type));
	}
      
      return FALSE;
    case SK_SPECIAL:
      if (s2->kind == SK_SPECIAL)
	{
	  return (s1->info->spec == s2->info->spec);
	}
      return FALSE;
    case SK_RESULT:
      return (s2->kind == SK_RESULT);
    default:
      return FALSE;
    }
  BADEXIT;
}
		
sRef
sRef_fixOuterRef (/*@returned@*/ sRef s)
{
  sRef root = sRef_getRootBase (s);

  if (sRef_isCvar (root))
    {
      uentry ue = usymtab_getRefQuiet (root->info->cvar->lexlevel, 
				       root->info->cvar->index);

      if (uentry_isValid (ue))
	{
	  sRef uref = uentry_getSref (ue);
	  sRef sr = sRef_fixBase (s, uref);

	  return (sr);
	}
      else
	{
	  llcontbug (message ("sRef_fixOuterRef: undefined: %q", sRef_unparseDebug (s)));
	  return (s);
	}
    }

  return (s);
}

void
sRef_storeState (sRef s)
{
  if (sRef_isInvalid (s)) return;

  s->oaliaskind = s->aliaskind;
  s->oexpkind = s->expkind;
}
  
static void sRef_resetStateAux (sRef s, /*@unused@*/ fileloc loc)
{
  sRef_resetState (s);
}

void
sRef_resetState (sRef s)
{
  bool changed = FALSE;
  if (sRef_isInvalid (s)) return;

  
  if (s->oaliaskind == AK_KILLREF && !sRef_isParam (s))
    {
      /*
      ** killref is used in a kludgey way, to save having to add
      ** another alias kind (see usymtab_handleParams)
      */
 
      if (s->expkind != s->oexpkind)
	{
	  changed = TRUE;
	  s->expkind = s->oexpkind;
	}
    }
  else
    {
      if (s->expkind != s->oexpkind)
	{
	  changed = TRUE;
	  s->expkind = s->oexpkind;	  
	}

      if (s->aliaskind != s->oaliaskind
	  && s->aliaskind != AK_REFCOUNTED
	  && s->aliaskind != AK_REFS)
	{
	  changed = TRUE;
	  s->aliaskind = s->oaliaskind;
	  	}
    }

  if (changed)
    {
      sRef_clearDerived (s);
    }
  
  }

void
sRef_resetStateComplete (sRef s)
{
  sRef_innerAliasSetComplete (sRef_resetStateAux, s, fileloc_undefined);
}

/*@exposed@*/ sRef
sRef_fixBase (/*@returned@*/ sRef s, /*@returned@*/ sRef base)
{
  sRef tmp = sRef_undefined;
  sRef ret;

  if (sRef_isInvalid (s)) return s;
  if (sRef_isInvalid (base)) return base;

  switch (s->kind)
    {
    case SK_RESULT:
    case SK_PARAM:
    case SK_CVAR:
      ret = base;
      break;
    case SK_ARRAYFETCH:
      tmp = sRef_fixBase (s->info->arrayfetch->arr, base);

      if (s->info->arrayfetch->indknown)
	{
	  ret = sRef_makeArrayFetchKnown (tmp, s->info->arrayfetch->ind);
	}
      else
	{
	  ret = sRef_makeArrayFetch (tmp);
	}
      break;
    case SK_FIELD:
      tmp = sRef_fixBase (s->info->field->rec, base);
      ret = sRef_buildNCField (tmp, s->info->field->field);
      break;
    case SK_PTR:
      tmp = sRef_fixBase (s->info->ref, base);
      ret = sRef_makePointer (tmp);
      break;
    case SK_ADR:
      tmp = sRef_fixBase (s->info->ref, base);
      ret = sRef_makeAddress (tmp);
      break;
    case SK_CONJ:
      {
	sRef tmpb;

	tmp = sRef_fixBase (s->info->conj->a, base);
	tmpb = sRef_fixBase (s->info->conj->b, base);

	ret = sRef_makeConj (tmp, tmpb);
	break;
      }
      BADDEFAULT;
    }

  return ret;
}

static /*@exposed@*/ sRef 
sRef_fixDirectBase (sRef s, sRef base)
{
  sRef ret;

  
  if (sRef_isInvalid (s))
    {
            return sRef_undefined;
    }

  switch (s->kind)
    {
    case SK_ARRAYFETCH:
      if (s->info->arrayfetch->indknown)
	{
	  ret = sRef_makeArrayFetchKnown (base, s->info->arrayfetch->ind);
	}
      else
	{
	  ret = sRef_makeArrayFetch (base);
	}
      break;
    case SK_FIELD:
      ret = sRef_buildNCField (base, s->info->field->field);
      break;
    case SK_PTR:
            ret = sRef_makePointer (base);
            break;
    case SK_ADR:
      ret = sRef_makeAddress (base);
      break;
    case SK_CONJ:
      {
	sRef tmpa, tmpb;

	tmpa = sRef_fixDirectBase (s->info->conj->a, base);
	tmpb = sRef_fixDirectBase (s->info->conj->b, base);

	ret = sRef_makeConj (tmpa, tmpb);
	break;
      }
      BADDEFAULT;
    }

    sRef_copyState (ret, s);
    return ret;
}

bool
sRef_isAllocIndexRef (sRef s)
{
  return (sRef_isArrayFetch (s) && !(s->info->arrayfetch->indknown) 
	  && sRef_isAllocated (s->info->arrayfetch->arr));
}

void
sRef_showRefLost (sRef s)
{
  if (sRef_hasAliasInfoLoc (s))
    {
      llgenindentmsg (cstring_makeLiteral ("Original reference lost"),
		sRef_getAliasInfoLoc (s));
    }
}

void
sRef_showRefKilled (sRef s)
{
  if (sRef_hasStateInfoLoc (s))
    {
      llgenindentmsg (message ("Storage %q released", 
			       sRef_unparse (s)), sRef_getStateInfoLoc (s));
    }
}

void
sRef_showStateInconsistent (sRef s)
{
  if (sRef_hasStateInfoLoc (s))
    {
      llgenindentmsg
	(message ("Storage %qbecomes inconsistent (released on one branch)",
		  sRef_unparseOpt (s)), 
	 sRef_getStateInfoLoc (s));
    }
}

void
sRef_showStateInfo (sRef s)
{
  if (sRef_hasStateInfoLoc (s))
    {
      if (s->defstate == SS_DEAD)
	{
	  llgenindentmsg 
	    (message ("Storage %qis released", sRef_unparseOpt (s)),
	     sRef_getStateInfoLoc (s));
	}
      else if (s->defstate == SS_ALLOCATED || s->defstate == SS_DEFINED)
	{
	  llgenindentmsg 
	    (message ("Storage %qis %s", sRef_unparseOpt (s), 
		      sstate_unparse (s->defstate)),
	     sRef_getStateInfoLoc (s));
	}
      else if (s->defstate == SS_UNUSEABLE)
	{
	  llgenindentmsg 
	    (message ("Storage %qbecomes inconsistent (clauses merge with"
		      "%qreleased on one branch)",
		      sRef_unparseOpt (s), 
		      sRef_unparseOpt (s)),
	     sRef_getStateInfoLoc (s));
	}
      else 
	{
	  llgenindentmsg (message ("Storage %qbecomes %s", 
				   sRef_unparseOpt (s), 
				   sstate_unparse (s->defstate)),
			  sRef_getStateInfoLoc (s));
	}
    }
}

void
sRef_showExpInfo (sRef s)
{
  if (sRef_hasExpInfoLoc (s))
    {
      llgenindentmsg (message ("Storage %qbecomes %s", sRef_unparseOpt (s), 
			       exkind_unparse (s->expkind)),
		      sRef_getExpInfoLoc (s));
    }
}

void
sRef_showNullInfo (sRef s)
{
  if (sRef_hasNullInfoLoc (s) && sRef_isKnown (s))
    {
      switch (s->nullstate)
	{
	case NS_CONSTNULL:
	  {
	    fileloc loc = sRef_getNullInfoLoc (s);
	    
	    if (fileloc_isDefined (loc) && !fileloc_isLib (loc))
	      {
		llgenindentmsg 
		  (message ("Storage %qbecomes null", sRef_unparseOpt (s)),
		   loc);
	      }
	    break;
	  }
	case NS_DEFNULL:
	  {
	    fileloc loc = sRef_getNullInfoLoc (s);
	    
	    if (fileloc_isDefined (loc) && !fileloc_isLib (loc))
	      {
		llgenindentmsg (message ("Storage %qbecomes null", sRef_unparseOpt (s)),
				loc);
	      }
	    break;
	  }
	case NS_ABSNULL:
	case NS_POSNULL:
	  llgenindentmsg
	    (message ("Storage %qmay become null", sRef_unparseOpt (s)),
	     sRef_getNullInfoLoc (s));
	  break;
	case NS_NOTNULL:
	case NS_MNOTNULL:
	  llgenindentmsg
	    (message ("Storage %qbecomes not null", sRef_unparseOpt (s)),
	     sRef_getNullInfoLoc (s));
	  break;
	case NS_UNKNOWN:
	  llgenindentmsg
	    (message ("Storage %qnull state becomes unknown",
		      sRef_unparseOpt (s)),
	     sRef_getNullInfoLoc (s));
	  break;

	case NS_ERROR:
	  BADBRANCHCONT;
	  break;

	default:
	  llgenindentmsg
	    (message ("<error case> Storage %q becomes %s",
		      sRef_unparse (s), 
		      nstate_unparse (s->nullstate)),
	     sRef_getNullInfoLoc (s));
	  
	  break;
	}
    }
}

void
sRef_showAliasInfo (sRef s)
{
  if (sRef_hasAliasInfoLoc (s))
    {
      if (sRef_isFresh (s))
	{
	  llgenindentmsg 
	    (message ("Fresh storage %qallocated", sRef_unparseOpt (s)),
	     sRef_getAliasInfoLoc (s));
	}
      else 
	{
	  if (!sRef_isRefCounted (s))
	    {
	      llgenindentmsg 
		(message ("Storage %qbecomes %s", 
			  sRef_unparseOpt (s),
			  alkind_unparse (sRef_getAliasKind (s))),
		 sRef_getAliasInfoLoc (s));
	    }
	}
    }
}

void
sRef_mergeNullState (sRef s, nstate n)
{
  if (sRef_isValid (s))
    {
      nstate old;
      
      old = s->nullstate;
      
      if (n != old && n != NS_UNKNOWN)
	{
	  	  
	  s->nullstate = n;
	  s->nullinfo = alinfo_updateLoc (s->nullinfo, g_currentloc);
	}
    }
  else
    {
      llbuglit ("sRef_mergeNullState: invalid");
    }
}

bool
sRef_possiblyNull (sRef s)
{
  if (sRef_isValid (s))
    {
      if (s->nullstate == NS_ABSNULL)
	{
	  ctype rct = ctype_realType (s->type);

	  if (ctype_isAbstract (rct))
	    {
	      return FALSE;
	    }
	  else
	    {
	      if (ctype_isUser (rct))
		{
		  uentry ue = usymtab_getTypeEntry (ctype_typeId (rct));
		  
		  return (nstate_possiblyNull
			  (sRef_getNullState (uentry_getSref (ue))));
		}
	      else
		{
		  return FALSE;
		}
	    }
	}
      else
	{
	  return nstate_possiblyNull (s->nullstate);
	}
    }

  return FALSE;
}

cstring
sRef_getScopeName (sRef s)
{
  sRef base = sRef_getRootBase (s);

  if (sRef_isRealGlobal (base))
    {
      return (cstring_makeLiteralTemp ("Global"));
    }
  else if (sRef_isFileStatic (base))
    {
      return (cstring_makeLiteralTemp ("Static"));
    }
  else
    {
      return (cstring_makeLiteralTemp ("Local"));
    }
}

cstring
sRef_unparseScope (sRef s)
{
  sRef base = sRef_getRootBase (s);

  if (sRef_isRealGlobal (base))
    {
      return (cstring_makeLiteralTemp ("global"));
    }
  else if (sRef_isFileStatic (base))
    {
      return (cstring_makeLiteralTemp ("file static"));
    }
  else
    {
      BADEXIT;
    }
}

int
sRef_getScope (sRef s)
{
  llassert (sRef_isValid (s));

  if (sRef_isCvar (s))
    {
      return s->info->cvar->lexlevel;
    }
  else if (sRef_isParam (s))
    {
      return paramsScope;
    }
  else
    {
      return fileScope;
    }
}

bool
sRef_isDead (sRef s)
{
  return (sRef_isValid (s) && (s)->defstate == SS_DEAD);
}

bool
sRef_isDeadStorage (sRef s)
{
  if (sRef_isValid (s))
    {
      if (s->defstate == SS_DEAD
	  || s->defstate == SS_UNUSEABLE
	  || s->defstate == SS_UNDEFINED
	  || s->defstate == SS_UNKNOWN)
	{
	  return TRUE;
	}
      else 
	{
	  return (sRef_isDefinitelyNull (s));
	}
    }
  else
    {
      return FALSE;
    }
}

bool
sRef_isPossiblyDead (sRef s)
{
  return (sRef_isValid (s) && s->defstate == SS_HOFFA);
}

bool sRef_isStateLive (sRef s)
{
  if (sRef_isValid (s))
    {
      sstate ds = s->defstate;

      return (!(ds == SS_UNDEFINED 
		|| ds == SS_DEAD
		|| ds == SS_UNUSEABLE
		|| ds == SS_HOFFA));
    }
  else
    {
      return FALSE;
    }
}


bool sRef_isStateUndefined (sRef s)
{
  return ((sRef_isValid(s)) && ((s)->defstate == SS_UNDEFINED));
}

bool sRef_isJustAllocated (sRef s)
{
  if (sRef_isAllocated (s))
    {
      sRefSet_allElements (s->deriv, el)
	{
	  if (!(sRef_isStateUndefined (el) || sRef_isUnuseable (el)))
	    {
	      return FALSE;
	    }
	} end_sRefSet_allElements ;

      return TRUE;
    }

  return FALSE;
}

static bool
sRef_isAllocatedStorage (sRef s)
{
  if (sRef_isValid (s) && ynm_toBoolStrict (sRef_isReadable (s)))
    {
      return (ctype_isVisiblySharable (sRef_getType (s)));
    }
  else
    {
      return FALSE;
    }
}

bool
sRef_isUnuseable (sRef s)
{
  return ((sRef_isValid(s)) && ((s)->defstate == SS_UNUSEABLE));
}

bool
sRef_perhapsNull (sRef s)
{
  if (sRef_isValid (s))
    {
      if (s->nullstate == NS_ABSNULL)
	{
	  ctype rct = ctype_realType (s->type);

	  if (ctype_isAbstract (rct))
	    {
	      return FALSE;
	    }
	  else
	    {
	      if (ctype_isUser (rct))
		{
		  uentry ue = usymtab_getTypeEntry (ctype_typeId (rct));

		  return (nstate_perhapsNull 
			  (sRef_getNullState (uentry_getSref (ue))));
		}
	      else
		{
		  return FALSE;
		}
	    }
	}
      else
	{
	  return nstate_perhapsNull (s->nullstate);
	}
    }

  return FALSE;
}

/*
** definitelyNull --- called when TRUE is good
*/

bool 
sRef_definitelyNull (sRef s)
{
  return (sRef_isValid (s)
	  && (s->nullstate == NS_DEFNULL || s->nullstate == NS_CONSTNULL));
}

/*
** based on sRef_similar
*/

void
sRef_setDerivNullState (sRef set, sRef guide, nstate ns)
{
  if (sRef_isValid (set))
    {
      sRef deriv = sRef_getDeriv (set, guide);
      
      if (sRef_isValid (deriv))
	{
	  deriv->nullstate = ns;
	}
    }
}

static /*@exposed@*/ sRef
sRef_getDeriv (/*@returned@*/ /*@notnull@*/ sRef set, sRef guide)
{
  llassert (sRef_isValid (set));
  llassert (sRef_isValid (guide));

  switch (guide->kind)
    {
    case SK_CVAR:
      llassert (set->kind == SK_CVAR);
      
      return set;

    case SK_PARAM:
      llassert (set->kind == guide->kind);
      llassert (set->info->paramno == guide->info->paramno);

      return set;

    case SK_ARRAYFETCH:

      if (set->kind == SK_ARRAYFETCH
	  && (sRef_similar (set->info->arrayfetch->arr,
			    guide->info->arrayfetch->arr)))
	{
	  return set;
	}
      else
	{
	  return (sRef_makeAnyArrayFetch 
		  (sRef_getDeriv (set, guide->info->arrayfetch->arr)));
	}

    case SK_PTR:
      
      if (set->kind == SK_PTR && sRef_similar (set->info->ref, guide->info->ref))
	{
	  return set;
	}
      else
	{
	  return (sRef_makePointer (sRef_getDeriv (set, guide->info->ref)));
	}
      
    case SK_FIELD:
      
      if ((set->kind == SK_FIELD &&
	   (sRef_similar (set->info->field->rec, guide->info->field->rec) &&
	    cstring_equal (set->info->field->field, guide->info->field->field))))
	{
	  return set;
	}
      else
	{
	  return (sRef_makeField (sRef_getDeriv (set, guide->info->field->rec),
				  guide->info->field->field));
	}
    case SK_ADR:
      
      if ((set->kind == SK_ADR) && sRef_similar (set->info->ref, guide->info->ref))
	{
	  return set;
	}
      else
	{
	  return (sRef_makeAddress (sRef_getDeriv (set, guide->info->ref)));
	}

    case SK_CONJ:
      
            return sRef_undefined;

    case SK_RESULT:
    case SK_SPECIAL:
    case SK_UNCONSTRAINED:
    case SK_TYPE:
    case SK_CONST:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_DERIVED:
    case SK_EXTERNAL:
      return sRef_undefined;
    }

  BADEXIT;
}
      
/*
** sRef_aliasCheckPred
**
** A confusing but spiffy function:
**
**    Calls predf (s, e, text, <alias>) on s and all of s's aliases
**    (unless checkAliases (s) is FALSE).
**
**    For alias calls, calls as
**          predf (alias, e, text, s)
*/

void
sRef_aliasCheckPred (bool (predf) (sRef, exprNode, sRef, exprNode),
		     /*@null@*/ bool (checkAliases) (sRef),
		     sRef s, exprNode e, exprNode err)
{
  bool error = (*predf)(s, e, sRef_undefined, err);
  
  
  if (checkAliases != NULL && !(checkAliases (s)))
    {
      /* don't check aliases */
    }
  else
    {
      sRefSet aliases = usymtab_allAliases (s);

      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      if (!sRef_similar (current, s)
		  || (error && sRef_sameName (current, s)))
		{
		  (void) (*predf)(current, e, s, err);
		}
	      }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
    }
}

/*
** return TRUE iff predf (s) is true for s or any alias of s
*/

bool
sRef_aliasCheckSimplePred (sRefTest predf, sRef s)
{
    
  if ((*predf)(s))
    {
      return TRUE;
    }
  else
    {
      sRefSet aliases;

      aliases = usymtab_allAliases (s);
      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      sRef cref = sRef_updateSref (current);
	      
	      /* Whoa! a very kludgey way to make sure the right sref is used
	      ** where there is a conditional symbol table.  I am beginning
	      ** to think that having a conditional symbol table wasn't such
	      ** a great idea.  ;(
	      */
	      
	      	      
	      if ((*predf)(cref))
		{
		  sRefSet_free (aliases);
		  return TRUE;
		}
	    }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
    }
  return FALSE;
}

bool
sRef_aliasCompleteSimplePred (bool (predf) (sRef), sRef s)
{
  sRefSet aliases;
  bool result = FALSE;
  
  
  aliases = usymtab_allAliases (s);
  
  if ((*predf)(s)) result = TRUE;

  
  sRefSet_realElements (aliases, current)
    {
      if (sRef_isValid (current))
	{
	  	  current = sRef_updateSref (current);
	  	  if ((*predf)(current)) result = TRUE;
	}
    } end_sRefSet_realElements;
  
  sRefSet_free (aliases);
  return result;
}

static void
sRef_aliasSetComplete (void (predf) (sRef, fileloc), sRef s, fileloc loc)
{
  sRefSet aliases;
  
  aliases = usymtab_allAliases (s);

  (*predf)(s, loc);

  sRefSet_realElements (aliases, current)
    {
      if (sRef_isValid (current))
	{
	  current = sRef_updateSref (current);
	  	  ((*predf)(current, loc));
	}
    } end_sRefSet_realElements;

  sRefSet_free (aliases);
}

static void
sRef_aliasSetCompleteParam (void (predf) (sRef, alkind, fileloc), sRef s, 
			    alkind kind, fileloc loc)
{
  sRefSet aliases;

  
  if (sRef_isDeep (s))
    {
      aliases = usymtab_allAliases (s);
    }
  else
    {
      aliases = usymtab_aliasedBy (s);
    }

  (*predf)(s, kind, loc);

  sRefSet_realElements (aliases, current)
    {
      if (sRef_isValid (current))
	{
	  current = sRef_updateSref (current);
	  	  ((*predf)(current, kind, loc));
	}
    } end_sRefSet_realElements;

  sRefSet_free (aliases);
}

static void
sRef_innerAliasSetComplete (void (predf) (sRef, fileloc), sRef s, fileloc loc)
{
  sRef inner;
  sRefSet aliases;
  ctype ct;

  if (!sRef_isValid (s)) return;

  
  /*
  ** Type equivalence checking is necessary --- there might be casting.
  */

  (*predf)(s, loc);

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CVAR:
    case SK_PARAM:
      break;
    case SK_PTR:
      inner = s->info->ref;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);

      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  sRef ptr = sRef_makePointer (current);
		  
		  ((*predf)(ptr, loc));
		}
	    }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
      break;
    case SK_ARRAYFETCH:
      inner = s->info->arrayfetch->arr;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);

      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  		  
		  if (s->info->arrayfetch->indknown)
		    {
		      sRef af = sRef_makeArrayFetchKnown (current, s->info->arrayfetch->ind);
		      
		      ((*predf)(af, loc));
		    }
		  else
		    {
		      sRef af = sRef_makeArrayFetch (current);
		      
		      ((*predf)(af, loc));
		    }
		}
	    }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
      break;
    case SK_FIELD:
      inner = s->info->field->rec;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);

      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  sRef f = sRef_makeField (current, s->info->field->field);
		  
		  ((*predf)(f, loc));
		}
	    }
	} end_sRefSet_realElements;
      
      sRefSet_free (aliases);
      break;
    case SK_CONJ:
      sRef_innerAliasSetComplete (predf, s->info->conj->a, loc);
      sRef_innerAliasSetComplete (predf, s->info->conj->b, loc);
      break;
    case SK_SPECIAL:
    case SK_ADR:
    case SK_TYPE:
    case SK_CONST:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_DERIVED:
    case SK_EXTERNAL:
    case SK_RESULT:
      break;
    }
}

static void
sRef_innerAliasSetCompleteParam (void (predf) (sRef, sRef), sRef s, sRef t)
{
  sRef inner;
  sRefSet aliases;
  ctype ct;

  if (!sRef_isValid (s)) return;

  
  /*
  ** Type equivalence checking is necessary --- there might be casting.
  */

  (*predf)(s, t);

  switch (s->kind)
    {
    case SK_UNCONSTRAINED:
    case SK_CVAR:
    case SK_PARAM:
      break;
    case SK_PTR:
      inner = s->info->ref;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);
      
      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  sRef ptr = sRef_makePointer (current);
		  
		  ((*predf)(ptr, t));
		}
	    }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
      break;
    case SK_ARRAYFETCH:
      inner = s->info->arrayfetch->arr;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);

      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  		  
		  if (s->info->arrayfetch->indknown)
		    {
		      sRef af = sRef_makeArrayFetchKnown (current, s->info->arrayfetch->ind);
		      
		      ((*predf)(af, t));
		    }
		  else
		    {
		      sRef af = sRef_makeArrayFetch (current);
		      
		      ((*predf)(af, t));
		    }
		}
	    }
	} end_sRefSet_realElements;

      sRefSet_free (aliases);
      break;
    case SK_FIELD:
      inner = s->info->field->rec;
      aliases = usymtab_allAliases (inner);
      ct = sRef_getType (inner);

      
      sRefSet_realElements (aliases, current)
	{
	  if (sRef_isValid (current))
	    {
	      current = sRef_updateSref (current);
	      
	      if (ctype_equal (ct, sRef_getType (current)))
		{
		  sRef f = sRef_makeField (current, s->info->field->field);
		  
		  ((*predf)(f, t));
		}
	    }
	} end_sRefSet_realElements;
      
      sRefSet_free (aliases);
      break;
    case SK_CONJ:
      sRef_innerAliasSetCompleteParam (predf, s->info->conj->a, t);
      sRef_innerAliasSetCompleteParam (predf, s->info->conj->b, t);
      break;
    case SK_SPECIAL:
    case SK_ADR:
    case SK_TYPE:
    case SK_CONST:
    case SK_NEW:
    case SK_UNKNOWN:
    case SK_OBJECT:
    case SK_DERIVED:
    case SK_EXTERNAL:
    case SK_RESULT:
      break;
    }
}

static void sRef_combineExKinds (/*@notnull@*/ sRef res, /*@notnull@*/ sRef other)
{
  exkind a1 = sRef_getExKind (res);
  exkind a2 = sRef_getExKind (other);

  if (a1 == a2 || a2 == XO_UNKNOWN) 
    {
      ;
    }
  else if (a1 == XO_UNKNOWN) 
    { 
      res->expinfo = alinfo_update (res->expinfo, other->expinfo);
      res->expkind = a2;
    }
  else
    {
      res->expkind = XO_OBSERVER;
    }
}

/*
** Currently, this is a very ad hoc implementation, with lots of fixes to
** make real code work okay.  I need to come up with some more general
** rules or principles here.
*/

static void 
  sRef_combineAliasKindsError (/*@notnull@*/ sRef res, 
			       /*@notnull@*/ sRef other, 
			       clause cl, fileloc loc)
{
  bool hasError = FALSE;
  alkind ares = sRef_getAliasKind (res);
  alkind aother = sRef_getAliasKind (other);

  if (alkind_isDependent (ares))
    {
      if (aother == AK_KEPT)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	  res->aliaskind = AK_KEPT;      
	}
      else 
	{
	  if (aother == AK_LOCAL || aother == AK_STATIC 
	      || alkind_isTemp (aother))
	    {
	      res->aliaskind = AK_DEPENDENT;
	    }
	}
    }
  else if (alkind_isDependent (aother))
    {
      if (ares == AK_KEPT)
	{
	  res->aliaskind = AK_KEPT;      
	}
      else 
	{
	  if (ares == AK_LOCAL || ares == AK_STATIC || alkind_isTemp (ares))
	    {
	      res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	      res->aliaskind = AK_DEPENDENT;
	    }
	}
    }
  else if ((ares == AK_LOCAL || ares == AK_UNIQUE
	    || ares == AK_STATIC || alkind_isTemp (ares))
	   && sRef_isFresh (other))
    {
      /*
      ** cases like: if (s == NULL) s = malloc...;
      **    don't generate errors
      */
      
      if (usymtab_isAltProbablyDeepNull (res))
	{
	  res->aliaskind = ares;
	}
      else
	{
	  hasError = TRUE; 
	}
    }
  else if ((aother == AK_LOCAL || aother == AK_UNIQUE
	    || aother == AK_STATIC || alkind_isTemp (aother))
	   && sRef_isFresh (res))
    {
      /*
      ** cases like: if (s == NULL) s = malloc...;
      **    don't generate errors
      */
      
      if (usymtab_isProbableDeepNull (other))
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	  res->aliaskind = aother;
	}
      else
	{
	  hasError = TRUE;
	}
    }
  else if (ares == AK_NEWREF && aother == AK_REFCOUNTED 
	   && sRef_isConst (other))
    {
      res->aliaskind = AK_NEWREF;
    }
  else if (aother == AK_NEWREF && ares == AK_REFCOUNTED
	   && sRef_isConst (res))
    {
      res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
      res->aliaskind = AK_NEWREF;
    }
  else if (sRef_isLocalVar (res)
	   && ((ares == AK_KEPT && aother == AK_LOCAL)
	       || (aother == AK_KEPT && ares == AK_LOCAL)))
    {
      res->aliaskind = AK_KEPT;
    }
  else
    {
      hasError = TRUE;
    }

  if (hasError)
    {
      if (sRef_isThroughArrayFetch (res))
	{
	  if (optgenerror2 
	      (FLG_BRANCHSTATE, FLG_STRICTBRANCHSTATE,
	       message
	       ("Clauses exit with %q possibly referencing %s storage %s, "
		"%s storage %s", 
		sRef_unparse (res),
		alkind_unparse (aother),
		clause_nameTaken (cl),
		alkind_unparse (ares),
		clause_nameAlternate (cl)),
	       loc))
	    {
	      sRef_showAliasInfo (res);
	      sRef_showAliasInfo (other);
	      res->aliaskind = AK_ERROR;
	    }
	  else
	    {
	      if (ares == AK_KEPT || aother == AK_KEPT)
		{
		  sRef_maybeKill (res, loc);
		  		}
	    }
	}
      else 
	{
	  if (optgenerror 
	      (FLG_BRANCHSTATE,
	       message ("Clauses exit with %q referencing %s storage %s, "
			"%s storage %s", 
			sRef_unparse (res),
			alkind_unparse (aother),
			clause_nameTaken (cl),
			alkind_unparse (ares),
			clause_nameAlternate (cl)),
	       loc))
	    {
	      sRef_showAliasInfo (res);
	      sRef_showAliasInfo (other);
	      
	      res->aliaskind = AK_ERROR;
	    }
	}
      
      res->aliaskind = (sRef_isLocalVar (res) ? AK_LOCAL : AK_UNKNOWN);
    }
}

static void 
  sRef_combineAliasKinds (/*@notnull@*/ sRef res, /*@notnull@*/ sRef other, 
			  clause cl, fileloc loc)
{
  alkind ares = sRef_getAliasKind (res);
  alkind aother = sRef_getAliasKind (other);

  if (alkind_equal (ares, aother)
      || aother == AK_UNKNOWN
      || aother == AK_ERROR)
    {
      ; /* keep current state */
    }
  else if (sRef_isDead (res) || sRef_isDead (other))
    {
      /* dead error reported (or storage is dead) */
      res ->aliaskind = AK_ERROR; 
    }
  else if (ares == AK_UNKNOWN || ares == AK_ERROR
	   || sRef_isStateUndefined (res))
    { 
      res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
      res->aliaskind = aother;  
    }
  else if (sRef_isStateUndefined (other))
    {
      ;
    }
  else if (((ares == AK_UNIQUE || alkind_isTemp (ares))
	    && aother == AK_LOCAL) 
	   || ((aother == AK_UNIQUE || alkind_isTemp (aother))
	       && ares == AK_LOCAL))
    {
      if (ares != AK_LOCAL)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}

      res->aliaskind = AK_LOCAL;
    }
  else if ((ares == AK_OWNED && aother == AK_FRESH) 
	   || (aother == AK_OWNED && ares == AK_FRESH))
    {
      if (ares != AK_FRESH)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}
      
      res->aliaskind = AK_FRESH;
    }
  else if ((ares == AK_KEEP && aother == AK_FRESH) ||
	   (aother == AK_KEEP && ares == AK_FRESH))
    {
      if (ares != AK_KEEP)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}
      
      res->aliaskind = AK_KEEP;
    }
  else if ((ares == AK_LOCAL && aother == AK_STACK) ||
	   (aother == AK_LOCAL && ares == AK_STACK))
    {
      if (ares != AK_STACK)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}

      res->aliaskind = AK_STACK;
    }
  else if ((ares == AK_LOCAL
	    && (aother == AK_OWNED && sRef_isLocalVar (other)))
	   || (aother == AK_LOCAL 
	       && (ares == AK_OWNED && sRef_isLocalVar (res))))
    {
      if (ares != AK_LOCAL)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}

      res->aliaskind = AK_LOCAL;
    }
  else if ((ares == AK_FRESH && alkind_isOnly (aother))
	   || (aother == AK_FRESH && alkind_isOnly (ares)))
    {
      res->aliaskind = AK_FRESH;
    }
  else if ((aother == AK_FRESH && sRef_definitelyNull (res))
	   || (ares == AK_FRESH && sRef_definitelyNull (other)))
    {
      if (ares != AK_FRESH)
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	  res->aliaskind = AK_FRESH;
	}
    }
  else if ((sRef_isFresh (res) && sRef_isConst (other))
	   || (sRef_isFresh (other) && sRef_isConst (res)))
    {
      /*
      ** for NULL constantants
      ** this is bogus!
      */

      if (!sRef_isFresh (res))
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	}

      res->aliaskind = AK_FRESH;
    }
  else if ((alkind_isStatic (aother) && sRef_isConst (res))
	   || (alkind_isStatic (ares) && sRef_isConst (other)))
    {
      if (!alkind_isStatic (ares))
	{
	  res->aliasinfo = alinfo_update (res->aliasinfo, other->aliasinfo);
	  res->aliaskind = AK_STATIC;
	}
    }
  else
    {
      sRef_combineAliasKindsError (res, other, cl, loc);
    }
}

static void sRef_combineDefState (/*@notnull@*/ sRef res, 
				  /*@notnull@*/ sRef other)
{
  sstate s1 = res->defstate;
  sstate s2 = other->defstate;
  bool flip = FALSE;

  if (s1 == s2 || s2 == SS_UNKNOWN)
    {
      ;
    }
  else if (s1 == SS_UNKNOWN)
    {
      flip = TRUE;
    }
  else
    {
      switch (s1)
	{
	case SS_FIXED:   
	  if (s2 == SS_DEFINED) 
	    {
	      break;
	    }
	  else
	    {
	      llcontbuglit ("ssfixed: not implemented");
	      flip = TRUE;
	    }
	  break;
	case SS_DEFINED: 
	  flip = TRUE;
	  break;
	case SS_PDEFINED:
	case SS_ALLOCATED: 
	  flip = (s2 != SS_DEFINED);
	  break;
	case SS_HOFFA:
	case SS_RELDEF:
	case SS_UNUSEABLE: 
	case SS_UNDEFINED: 
	case SS_PARTIAL:
	case SS_UNDEFGLOB:
	case SS_KILLED:
	case SS_DEAD:      
	case SS_SPECIAL: 
	  break;
	BADDEFAULT;
	}
    }

  if (flip)
    {
      res->definfo = alinfo_update (res->definfo, other->definfo);
      res->defstate = s2;
    }
}

extern /*@notnull@*/ sRef sRef_getConjA (sRef s)
{
  sRef ret;
  llassert (sRef_isConj (s));

  ret = s->info->conj->a;
  llassert (ret != NULL);
  return ret;
}

extern /*@notnull@*/ sRef sRef_getConjB (sRef s)
{
  sRef ret;
  llassert (sRef_isConj (s));

  ret = s->info->conj->b;
  llassert (ret != NULL);
  return ret;
}
  
extern /*@exposed@*/ sRef sRef_makeArrow (sRef s, /*@dependent@*/ cstring f)
{
  sRef p;
  sRef ret;

    p = sRef_makePointer (s);
  ret = sRef_makeField (p, f);
    return ret;
}

extern /*@exposed@*/ sRef sRef_buildArrow (sRef s, cstring f)
{
  sRef p;
  sRef ret;

  p = sRef_buildPointer (s);
  ret = sRef_buildField (p, f);
  
  return ret;
}

static /*@null@*/ sinfo sinfo_copy (/*@notnull@*/ sRef s)
{
  sinfo ret;

  switch (s->kind)
    {
    case SK_CVAR:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->cvar = (cref) dmalloc (sizeof (*ret->cvar));
      ret->cvar->lexlevel = s->info->cvar->lexlevel; 
      ret->cvar->index = s->info->cvar->index; 
      break;

    case SK_PARAM:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->paramno = s->info->paramno; 
      break;

    case SK_ARRAYFETCH:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->arrayfetch = (ainfo) dmalloc (sizeof (*ret->arrayfetch));
      ret->arrayfetch->indknown = s->info->arrayfetch->indknown;
      ret->arrayfetch->ind = s->info->arrayfetch->ind;
      ret->arrayfetch->arr = s->info->arrayfetch->arr;
      break;

    case SK_FIELD:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->field = (fldinfo) dmalloc (sizeof (*ret->field));
      ret->field->rec = s->info->field->rec;
      ret->field->field = s->info->field->field; 
      break;

    case SK_OBJECT:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->object = s->info->object;
      break;

    case SK_PTR:
    case SK_ADR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->ref = s->info->ref;	 
      break;

    case SK_CONJ:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->conj = (cjinfo) dmalloc (sizeof (*ret->conj));
      ret->conj->a = s->info->conj->a;
      ret->conj->b = s->info->conj->b;
      break;
    case SK_SPECIAL:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->spec = s->info->spec;
      break;
    case SK_UNCONSTRAINED:
    case SK_NEW:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->fname = s->info->fname;
      break;
    case SK_RESULT:
    case SK_CONST:
    case SK_TYPE:
    case SK_UNKNOWN:
      llassertprint (s->info == NULL, ("s = %s", sRef_unparse (s)));
      ret = NULL;
      break;
    }

  return ret;
}

static /*@null@*/ sinfo sinfo_fullCopy (/*@notnull@*/ sRef s)
{
  sinfo ret;

  /*
  ** Since its a full copy, only storage is assigned
  ** to dependent fields.
  */
  /*@-onlytrans@*/

  switch (s->kind)
    {
    case SK_CVAR:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->cvar = (cref) dmalloc (sizeof (*ret->cvar));
      ret->cvar->lexlevel = s->info->cvar->lexlevel; 
      ret->cvar->index = s->info->cvar->index; 
      break;

    case SK_PARAM:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->paramno = s->info->paramno; 
      break;

    case SK_ARRAYFETCH:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->arrayfetch = (ainfo) dmalloc (sizeof (*ret->arrayfetch));
      ret->arrayfetch->indknown = s->info->arrayfetch->indknown;
      ret->arrayfetch->ind = s->info->arrayfetch->ind;
      ret->arrayfetch->arr = sRef_saveCopy (s->info->arrayfetch->arr);
      break;

    case SK_FIELD:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->field = (fldinfo) dmalloc (sizeof (*ret->field));
      ret->field->rec = sRef_saveCopy (s->info->field->rec);
      ret->field->field = s->info->field->field; 
      break;

    case SK_OBJECT:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->object = s->info->object;
      break;

    case SK_PTR:
    case SK_ADR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->ref = sRef_saveCopy (s->info->ref);	 
      break;

    case SK_CONJ:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->conj = (cjinfo) dmalloc (sizeof (*ret->conj));
      ret->conj->a = sRef_saveCopy (s->info->conj->a);
      ret->conj->b = sRef_saveCopy (s->info->conj->b);
      break;
    case SK_SPECIAL:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->spec = s->info->spec;
      break;
    case SK_NEW:
    case SK_UNCONSTRAINED:
      ret = (sinfo) dmalloc (sizeof (*ret));
      ret->fname = s->info->fname;
      break;
    case SK_CONST:
    case SK_TYPE:
    case SK_RESULT:
    case SK_UNKNOWN:
      llassert (s->info == NULL);
      ret = NULL;
      break;
    }
  /*@=onlytrans@*/ 
  return ret;
}


static void 
  sinfo_update (/*@notnull@*/ /*@exposed@*/ sRef res, 
		/*@notnull@*/ /*@exposed@*/ sRef other)
{
  llassert (res->kind == other->kind);

  switch (res->kind)
    {
    case SK_CVAR:
      res->info->cvar->lexlevel = other->info->cvar->lexlevel; 
      res->info->cvar->index = other->info->cvar->index; 
      break;

    case SK_PARAM:
      res->info->paramno = other->info->paramno; 
      break;

    case SK_ARRAYFETCH:
      res->info->arrayfetch->indknown = other->info->arrayfetch->indknown;
      res->info->arrayfetch->ind = other->info->arrayfetch->ind;
      res->info->arrayfetch->arr = other->info->arrayfetch->arr;
      break;

    case SK_FIELD:
      res->info->field->rec = other->info->field->rec;
      res->info->field->field = other->info->field->field; 
      break;

    case SK_OBJECT:
      res->info->object = other->info->object;
      break;

    case SK_PTR:
    case SK_ADR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      res->info->ref = other->info->ref;	 
      break;

    case SK_CONJ:
      res->info->conj->a = other->info->conj->a;
      res->info->conj->b = other->info->conj->b;
      break;

    case SK_SPECIAL:
      res->info->spec = other->info->spec;
      break;

    case SK_NEW:
    case SK_UNCONSTRAINED:
      res->info->fname = other->info->fname;
      break;

    case SK_CONST:
    case SK_TYPE:
    case SK_UNKNOWN:
    case SK_RESULT:
      llassert (res->info == NULL);
      break;
    }
}

static void sinfo_free (/*@special@*/ /*@temp@*/ /*@notnull@*/ sRef s)
   /*@uses s->kind, s->info@*/
   /*@releases s->info@*/ 
{
  switch (s->kind)
    {
    case SK_CVAR:
      sfree (s->info->cvar);
      break;

    case SK_PARAM:
      break;

    case SK_ARRAYFETCH:
      sfree (s->info->arrayfetch);
      break;

    case SK_FIELD:
      sfree (s->info->field); 
      break;

    case SK_OBJECT:
      break;

    case SK_PTR:
    case SK_ADR:
    case SK_DERIVED:
    case SK_EXTERNAL:
      break;

    case SK_CONJ:
      sfree (s->info->conj);
      break;

    case SK_UNCONSTRAINED:
    case SK_SPECIAL:
    case SK_CONST:
    case SK_NEW:
    case SK_TYPE:
    case SK_UNKNOWN:
    case SK_RESULT:
      break;
    }

  sfree (s->info);
}

bool sRef_isNSLocalVar (sRef s)  
{
  if (sRef_isLocalVar (s))
    {
      uentry ue = sRef_getUentry (s);

      return (!uentry_isStatic (ue));
    }
  else
    {
      return FALSE;
    }
}

bool sRef_isLocalVar (sRef s)  
{
  if (sRef_isValid(s))
    {
      return (s->kind == SK_CVAR 
	      && (s->info->cvar->lexlevel > fileScope));
    }
  
  return FALSE;
}

bool sRef_isRealLocalVar (sRef s)  
{
  if (sRef_isValid(s))
    {
      if (s->kind == SK_CVAR)
	{
	  if (s->info->cvar->lexlevel == functionScope)
	    {
	      uentry ue = sRef_getUentry (s);

	      if (uentry_isAnyParam (ue)
		  || uentry_isRefParam (ue))
		{
		  return FALSE;
		}
	      else
		{
		  return TRUE;
		}
	    }
	  else
	    {
	      return (s->info->cvar->lexlevel > functionScope);
	    }
	}
    }
  
  return FALSE;
}

bool sRef_isLocalParamVar (sRef s)  
{
  if (sRef_isValid(s))
    {
      return (s->kind == SK_PARAM
	      || (s->kind == SK_CVAR 
		  && (s->info->cvar->lexlevel > fileScope)));
    }
  
  return FALSE;
}

static speckind speckind_fromInt (int i)
{
  /*@+enumint@*/ 
  llassert (i >= SR_NOTHING && i <= SR_SYSTEM); 
  /*@=enumint@*/

  return ((speckind) i);
}

void sRef_combineNullState (/*@notnull@*/ sRef res, /*@notnull@*/ sRef other)
{
  nstate n1 = res->nullstate;
  nstate n2 = other->nullstate;
  bool flip = FALSE;
  nstate nn = n1;

  if (n1 == n2 || n2 == NS_UNKNOWN)
    {
      ;
    }
  else
    {
      /* note: n2 is not unknown or defnull */

      switch (n1)
	{
	case NS_ERROR:   nn = NS_ERROR; break;
	case NS_UNKNOWN: flip = TRUE; nn = n2; break; 
	case NS_POSNULL: break;
	case NS_DEFNULL: nn = NS_POSNULL; break;
	case NS_RELNULL: break;
	case NS_NOTNULL:  
	  if (n2 == NS_MNOTNULL)
	    {
	      ;
	    }
	  else 
	    { 
	      flip = TRUE;
	      nn = NS_POSNULL; 
	    }
	  break;
	case NS_MNOTNULL: 
	  if (n2 == NS_NOTNULL) 
	    {
	      nn = NS_NOTNULL; 
	    }
	  else 
	    {
	      flip = TRUE;
	      nn = NS_POSNULL; 
	    }
	  break;
	case NS_CONSTNULL:
	case NS_ABSNULL:
	  flip = TRUE;
	  nn = n2;
	}
    }
  
  if (flip)
    {
      res->nullinfo = alinfo_update (res->nullinfo, other->nullinfo);      
    }

  res->nullstate = nn;
}

cstring sRef_nullMessage (sRef s)
{
  llassert (sRef_isValid (s));

  switch (s->nullstate)
    {
    case NS_DEFNULL:
    case NS_CONSTNULL:
      return (cstring_makeLiteralTemp ("null"));
    default:
      return (cstring_makeLiteralTemp ("possibly null"));
    }
  BADEXIT;
}

/*@observer@*/ cstring sRef_ntMessage (sRef s)
{
  llassert (sRef_isValid (s));

  switch (s->nullstate)
    {
    case NS_DEFNULL:
    case NS_CONSTNULL:
      return (cstring_makeLiteralTemp ("not nullterminated"));
    default:
      return (cstring_makeLiteralTemp ("possibly non-nullterminated"));
    }
  BADEXIT;
}



sRef sRef_fixResultType (/*@returned@*/ sRef s, ctype typ, uentry ue)
{
  sRef tmp = sRef_undefined;
  sRef ret;

  llassert (sRef_isValid (s));

  switch (s->kind)
    {
    case SK_RESULT:
      s->type = typ;
      ret = s;
      break;
    case SK_ARRAYFETCH:
      {
	ctype ct;
	tmp = sRef_fixResultType (s->info->arrayfetch->arr, typ, ue);

	ct = ctype_realType (sRef_getType (tmp));

	
	if (ctype_isKnown (ct))
	  {
	    if (ctype_isAP (ct))
	      {
		;
	      }
	    else
	      {
		voptgenerror 
		  (FLG_TYPE,
		   message
		   ("Special clause indexes non-array (%t): *%q",
		    ct, sRef_unparse (s->info->arrayfetch->arr)),
		   uentry_whereLast (ue));
	      }
	  }

	tmp = sRef_fixResultType (s->info->arrayfetch->arr, typ, ue);

	if (s->info->arrayfetch->indknown)
	  {
	    ret = sRef_makeArrayFetchKnown (tmp, s->info->arrayfetch->ind);
	  }
	else
	  {
	    ret = sRef_makeArrayFetch (tmp);
	  }
      }
      break;
    case SK_FIELD:
      {
	sRef rec = sRef_fixResultType (s->info->field->rec, typ, ue);
	ctype ct = ctype_realType (sRef_getType (rec));

	if (ctype_isKnown (ct))
	  {
	    if (ctype_isSU (ct))
	      {
		if (uentry_isValid (uentryList_lookupField (ctype_getFields (ct), 
							    s->info->field->field)))
		  {
		    ;
		  }
		else
		  {
		    voptgenerror 
		      (FLG_TYPE,
		       message
		       ("Special clause accesses non-existent field of result: %q.%s",
			sRef_unparse (rec), s->info->field->field),
		       uentry_whereLast (ue));
		  }
	      }
	    else
	      {
		voptgenerror 
		  (FLG_TYPE,
		   message
		   ("Special clause accesses field of non-struct or union result (%t): %q.%s",
		    ct, sRef_unparse (rec), s->info->field->field),
		   uentry_whereLast (ue));
	      }
	  }
	
	ret = sRef_makeField (tmp, s->info->field->field);
	break;
      }
    case SK_PTR:
      {
	ctype ct;
	tmp = sRef_fixResultType (s->info->ref, typ, ue);

	ct = ctype_realType (sRef_getType (tmp));

	if (ctype_isKnown (ct))
	  {
	    if (ctype_isAP (ct))
	      {
		;
	      }
	    else
	      {
		voptgenerror 
		  (FLG_TYPE,
		   message
		   ("Special clause dereferences non-pointer (%t): *%q",
		    ct, sRef_unparse (s->info->ref)),
		   uentry_whereLast (ue));
	      }
	  }
	
	ret = sRef_makePointer (tmp);
	break;
      }
    case SK_ADR:
      voptgenerror 
	(FLG_TYPE,
	 message
	 ("Special clause uses & operator (not allowed): &%q", sRef_unparse (s->info->ref)),
	 uentry_whereLast (ue));
      ret = s;
      break;
    BADDEFAULT;
    }

  return ret;
}

bool sRef_isOnly (sRef s)
{
  return (sRef_isValid(s) && alkind_isOnly (s->aliaskind));
}

bool sRef_isDependent (sRef s) 
{
  return (sRef_isValid(s) && alkind_isDependent (s->aliaskind));
}

bool sRef_isOwned (sRef s)
{
  return (sRef_isValid (s) && (s->aliaskind == AK_OWNED));
}

bool sRef_isKeep (sRef s) 
{
  return (sRef_isValid (s) && (s->aliaskind == AK_KEEP));
}

bool sRef_isTemp (sRef s)
{
  return (sRef_isValid (s) && alkind_isTemp (s->aliaskind));
}

bool sRef_isLocalState (sRef s) 
{
  return (sRef_isValid (s) && (s->aliaskind == AK_LOCAL));
}

bool sRef_isUnique (sRef s)
{
  return (sRef_isValid (s) && (s->aliaskind == AK_UNIQUE));
}

bool sRef_isShared (sRef s) 
{
  return (sRef_isValid (s) && (s->aliaskind == AK_SHARED));
}

bool sRef_isExposed (sRef s) 
{
  return (sRef_isValid (s) && (s->expkind == XO_EXPOSED));
}

bool sRef_isObserver (sRef s) 
{
  return (sRef_isValid (s) && (s->expkind == XO_OBSERVER));
}

bool sRef_isFresh (sRef s) 
{
  return (sRef_isValid (s) && (s->aliaskind == AK_FRESH));
}

bool sRef_isDefinitelyNull (sRef s) 
{
  return (sRef_isValid (s) && (s->nullstate == NS_DEFNULL 
			       || s->nullstate == NS_CONSTNULL));
}

bool sRef_isAllocated (sRef s)
{
  return (sRef_isValid (s) && (s->defstate == SS_ALLOCATED));
}

bool sRef_isStack (sRef s)
{
  return (sRef_isValid (s) && (s->aliaskind == AK_STACK));
}

extern bool sRef_isNotNull (sRef s)
{
  return (sRef_isValid(s) && (s->nullstate == NS_MNOTNULL 
			      || s->nullstate == NS_NOTNULL));
}

/* start modifications */
struct _bbufinfo sRef_getNullTerminatedState (sRef p_s) {
   struct _bbufinfo BUFSTATE_UNKNOWN;
   BUFSTATE_UNKNOWN.bufstate = BB_NOTNULLTERMINATED;
   BUFSTATE_UNKNOWN.size = 0;
   BUFSTATE_UNKNOWN.len = 0;
   
   if (sRef_isValid(p_s))
      return p_s->bufinfo;
   return BUFSTATE_UNKNOWN; 
}

void sRef_setNullTerminatedState(sRef p_s) {
   if(sRef_isValid (p_s)) {
      p_s->bufinfo.bufstate = BB_NULLTERMINATED;
   } else {
      llfatalbug( message("sRef_setNT passed a invalid sRef\n"));
   }
}


void sRef_setPossiblyNullTerminatedState(sRef p_s) {
   if( sRef_isValid (p_s)) {
      p_s->bufinfo.bufstate = BB_POSSIBLYNULLTERMINATED;
   } else {
      llfatalbug( message("sRef_setPossNT passed a invalid sRef\n"));
   }
}

void sRef_setNotNullTerminatedState(sRef p_s) {
   if( sRef_isValid (p_s)) {
      p_s->bufinfo.bufstate = BB_NOTNULLTERMINATED;
   } else {
      llfatalbug( message("sRef_unsetNT passed a invalid sRef\n"));
   }
}

void sRef_setLen(sRef p_s, int len) {
   if( sRef_isValid (p_s) && sRef_isNullTerminated(p_s)) {
      p_s->bufinfo.len = len;
   } else {
      llfatalbug( message("sRef_setLen passed a invalid sRef\n"));
   }
}
    

void sRef_setSize(sRef p_s, int size) {
   if( sRef_isValid(p_s)) {
       p_s->bufinfo.size = size;
   } else {
      llfatalbug( message("sRef_setSize passed a invalid sRef\n"));
   }
}

void sRef_resetLen(sRef p_s) {
	if (sRef_isValid (p_s)) {
		p_s->bufinfo.len = 0;
	} else {
		llfatalbug (message ("sRef_setLen passed an invalid sRef\n"));
	}
}

/*drl7x 11/28/2000 */

bool sRef_isFixedArray (sRef p_s) {
  ctype c;
  c = sRef_getType (p_s);
  return ( ctype_isFixedArray (c) );
}

int sRef_getArraySize (sRef p_s) {
  ctype c;
  llassert (sRef_isFixedArray(p_s) );

  c = sRef_getType (p_s);

  return (ctype_getArraySize (c) );
}


