/*
* Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:
* Name        : libm_aliases.h
* Part of     : libm
* This Header contains the macro declarations which serves the purposes of
* weak and strong aliases.
* Version     : 1.0
*
*/



#ifndef __LIBM_ALIASES_H__
#define __LIBM_ALIASES_H__

#define frexpl 	frexp
#define nexttoward 	nextafter
#define nexttowardl	nextafter
#define nextafterl nextafter
#define ldexpl scalbn
#define	ldexpf scalbnf
#define scalbnl scalbn



/*
following are the aliases defined for the longdouble apis
*/
#define  	cbrtl  	cbrt
#define  	ceill  	ceil
#define  	erfl   	erf
#define 	erfcl  	erfc
#define 	expl   	exp
#define 	exp2l  	exp2
#define 	expm1l 	expm1
#define 	j0l  	j0
#define 	j1l  	j1
#define 	lrintl	lrint
#define 	lroundl	lround
#define 	roundl	round
#define 	logl	log
#define		log10l 	log10
#define		log1pl 	log1p
#define		logbl 	logb
#define		rintl 	rint
#define		sqrtl 	sqrt
#define		y0l 	y0
#define		y1l 	y1
#define		significandl 	significand
#define		nearbyintl 		nearbyint
#define 	finitel			finite
#define 	floorl			floor
#define 	fmal			fma
#define		fminl			fmin
#define 	gammal			gamma
#define 	lgammal			lgamma
#define		cosl			cos
#define		acosl			acos
#define		acoshl			acosh
#define		asinl			asin
#define		asinhl			asinh
#define		atanl			atan
#define		atanhl			atanh
#define		sinl			sin
#define		sinhl			sinh
#define		coshl			cosh
#define 	tanl			tan
#define 	tanhl			tanh
#define 	atan2l		atan2
#define		powl			pow
#define		dreml			drem
#define		fmodl			fmod
#define		hypotl			hypot
#define		remainderl		remainder
#define		jnl				jn
#define		ynl				yn
#define		scalbl			scalb
#define		modfl			modf
#define		remquol			remquo
#define		llrintl			llrint
#define		llroundl		llround

#endif /*__LIBM_ALIASES_H__*/
