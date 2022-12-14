// *********************************************************************
// **
// ** Projected Spherical Cap Maps
// ** template class 'PSCMaps' (class and aux funcs declaration)
// **
// ** Copyright (C) 2018 Carlos Ureña and Iliyan Georgiev
// ** Ported to HLSL by Christoph Peters
// **
// ** Licensed under the Apache License, Version 2.0 (the "License");
// ** you may not use this file except in compliance with the License.
// ** You may obtain a copy of the License at
// **
// **    http://www.apache.org/licenses/LICENSE-2.0
// **
// ** Unless required by applicable law or agreed to in writing, software
// ** distributed under the License is distributed on an "AS IS" BASIS,
// ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// ** See the License for the specific language governing permissions and
// ** limitations under the License.

// -----------------------------------------------------------------------------
// types

typedef float T;


// -----------------------------------------------------------------------------
// A struct for projected spherical cap maps evaluation state

struct PSCMaps
{
   bool // values defining the spherical cap type, and which map is being used
      fully_visible,     // true iif r <= cz     (sphere fully visible)
      partially_visible, // true iif -r < cz < r (sphere partially visible)
      center_below_hor,  // true iif -r <= cz < 0 (partially visible and sphere center below horizon)
      invisible ;        // true iif cz <= -r    (sphere completely invisible)

   T // areas (form factors)
      E,      // half of the ellipse area
      L ,     // half lune area, (it is 0 in the 'ellipse only' case)
      F ;     // total form factor: it is:
              //    0 -> when invisible
              //    2E -> when fully_visible (ellipse only)
              //    2L -> when partially_visible and center_below_hor (lune only)
              //    2(E+L) -> otherwise (partially_visible and not center_below_hor)

   T  // sinus and cosine of beta
      cos_beta,
      cos_beta_sq,
      sin_beta,
      sin_beta_abs ;

   T  // parameters defining the ellipse
      xe ,    // X coord. of ellipse center in the local reference frame
      ax,     // ellipse semi-minor axis (X direction)
      ay ;    // ellipse semi-major axis (Y direction)

   T // some precomputed constants
      axay2 , // == (ax*ay)/2
      ay_sq , // == ay^2
      xe_sq , // == xe^2
      r1maysq ; // root of (1-ay^2)

   T // parameters computed only when partially_visible ( there are tangency points)
      xl,        // X coord. of tangency point p (>0)
      yl ,       // Y coord. of tangency point p (>0)
      phi_l,     // == arctan(yl/(xe-xl)), only if 'partially_visible'
      AE_phi_l ; // == A_E(phi_l), only if 'partially_visible'
};


// ****************************************************************************
// Radial map related methods

// checks that the sphere is partially visible
// and that 'theta' is in the range (0,phi_l) (computes phi_l)
// truncates 'theta' if it is slightly off-range (to within epsilon)

void check_theta( inout T theta, T max_theta )
{
   theta = max( T(0.0), min( theta, max_theta )); // trunc
}
// ---------------------------------------------------------------------------
// eval 're(theta)' according to equation 45

T eval_rEll( PSCMaps map, T theta )
{
   check_theta( theta, T(3.14159265f) );

   const T sin_theta    = sin(theta);
   const T sin_theta_sq = sin_theta*sin_theta ;
   return map.ax / sqrt( T(1.0)-map.cos_beta_sq* sin_theta_sq );
}

// ---------------------------------------------------------------------------
// eval 'rc(theta)' according to equation 48

T eval_rCirc( PSCMaps map, T theta )
{
   check_theta( theta, map.phi_l );

   const T sin_theta    = sin(theta);
   const T sin_theta_sq = sin_theta*sin_theta ;
   const T cos_theta    = sqrt( T(1.0) - sin_theta_sq ); // sign is positive because theta < phi_l < PI/2
   return sqrt( T(1.0)- map.xe_sq * sin_theta_sq ) - map.xe*cos_theta ;
}
// ---------------------------------------------------------------------------
// evaluate Re according to expression 50  (sec.4)
// theta in [0,pi]
T eval_ArE( PSCMaps map, T theta )
{
   check_theta( theta, T(3.14159265f) );

   const T tt = abs(tan(theta));

   T pi2 = T(0.5)*3.14159265f;
   T result ;
   if ( theta <= pi2 )
      result = map.axay2*atan( map.sin_beta_abs*tt );
   else
      result = map.axay2*( 3.14159265f - atan( map.sin_beta_abs*tt ));

   return result ;
}

// ---------------------------------------------------------------------------
// evaluate ArC according to equation 25
// theta in [0,phi_l]

T eval_ArC( PSCMaps map, T theta )
{
   check_theta( theta, map.phi_l );

   const T z      = sin( theta ); // z == asin(theta)
   const T xez    = map.xe*z;
   const T z_sq   = z*z;
   const T xe_sq  = map.xe*map.xe ;

   // this is equivalent to, but faster than, eq. 25
   // (an 'asin' call is saved)
   return T(0.5)*
           ( theta
             - asin(xez)
             + xe_sq*z*sqrt( T(1.0)-z_sq )
             - xez*sqrt( T(1.0)- xe_sq*z_sq)
           );

}

// ---------------------------------------------------------------------------
// evals the radial integral, an optimized version of equation 25
// theta in [0,pi]

T eval_Ar( PSCMaps map, T theta )
{
   // ellipse only
   if ( map.fully_visible )
      return eval_ArE( map, theta  );

   // lune only
   if ( map.center_below_hor )
      return (theta <= map.phi_l)
         ? (eval_ArC( map, theta ) - eval_ArE( map, theta ))
         : map.L ;

    // ellipse+lune
   return  ( theta <= map.phi_l )
         ? eval_ArC( map, theta )
         : eval_ArE( map, theta ) + map.L ;
}

// ---------------------------------------------------------------------------
// evals the radial integrand, as defined in equation 20,
//   it is implemented in terms of 'eval_rEll' and 'eval_rCirc'
T eval_rad_integrand( PSCMaps map, T theta )
{
   // ellipse only, or: ellipse+lune and theta above phi_l
   if ( map.fully_visible || ( !map.center_below_hor && map.phi_l <= theta ) )
   {
      const T re = eval_rEll( map, theta ) ;
      return T(0.5)*re*re ;
   }

   // lune only
   if ( map.center_below_hor )
   {
      if ( theta <= map.phi_l )
      {
         const T rc = eval_rCirc( map, theta );
         const T re = eval_rEll( map, theta );
         return T(0.5)*( rc*rc - re*re );
      }
      else
         return T(0.0) ;
   }

   // ellipse+lune (theta is for sure below phi_l, see above)
   const T r = eval_rCirc( map, theta ) ;
   return T(0.5)*r*r ;

}
// ---------------------------------------------------------------------------

void eval_rmin_rmax( PSCMaps map, const T theta, out T rmin, out T rmax )
{
   const T theta_c = min( theta, T(3.14159265f) );

   // ellipse only, or: ellipse+lune and theta above phi_l
   if ( map.fully_visible || ( !map.center_below_hor && map.phi_l <= theta_c ) )
   {
      rmin = T(0.0);
      rmax = eval_rEll( map, theta_c );
   }
   else if ( map.center_below_hor ) // lune only
   {
      rmin = eval_rEll( map, theta_c );
      rmax = eval_rCirc( map, theta_c );
   }
   else // ellipse+lune (theta is for sure below phi_l, see above)
   {
      rmin = T(0.0);
      rmax = eval_rCirc( map, theta_c ) ;
   }

}

// --------------------------------------------------------------------------
// evaluation of the inverse area integral, for the radial map, in the ellipse
// only (full visible) case
//
T eval_ArE_inverse( PSCMaps map, T Ar_value )
{
   const T Ar_max_value = map.E ;

   Ar_value = max( T(0.0), min( Ar_value, Ar_max_value ) );

   const T ang = Ar_value/map.axay2 ;
   T pi2 = T(0.5)*T(3.14159265f) ;

   if ( ang <= pi2 )
      return atan(tan(ang)/map.sin_beta_abs);
   else
      return T(3.14159265f) + atan(tan(ang)/map.sin_beta_abs);
}

// -----------------------------------------------------------------------------
// function InverseNSB
//
// evaluate the inverse of a function whose derivative is > 0
//  Uses Newton method for root finding when possible,
//  and a combination of Binary search  and Secant methods
//  (at each step the best option is selected)
//
// returns the value 't' (in the range [0,t_max]) such that F(t) = Aobj
// it is assumed that F(t) in [0,A_max] for t in [0,t_max]
//                    0.0 <= Aobj <= A_max
//                    F(0.0) == 0.0, and F(t_max) == A_max
//                    F' = f
// where
//    F    : function whose inverse value we want to compute
//    f    : derivative of F  (>0 in [0,t_max])
//    t_max: maximum value for the result 't'
//    Aobj : desired value of F(t)
//    A_max: maximum value for A (minimum is 0.0)
//

T InverseNSB( PSCMaps map,
              const T t_max, const T Aobj, const T A_max )
{
   const T Ar_max_value = T(0.5)*map.F ;
   const T
      A      = max( T(0.0), min( Aobj, A_max ) ); // A is always positive
   T
      tn     = (A/A_max)*t_max, // current best estimation of result value 't'
      tn_min = T(0.0) ,         // current interval: minimum value
      tn_max = t_max ;          // current interval: maximum value


   int num_iters = 0 ;  // number of iterations so far


   T diff ;

   while( true )
   {
      const T Ftn  = eval_Ar( map, tn )/Ar_max_value;

      diff = Ftn - A ;

      // exit when done
      if ( abs( diff ) <= 1.0e-4f )
      {
         break ;
      }

      // compute derivative, we must trunc it so it there are no out-of-range instabilities
      const T ftn = eval_rad_integrand( map, tn )/Ar_max_value;   // we know f(yn) is never negative

      const T delta = -diff/ftn ;

      T tn_next = tn + delta ;

      if ( isnan(tn_next) || (tn_next < tn_min || tn_max < tn_next) )
      {
         // tn_next out of range

         // update interval
         bool left = ( 0.0 < diff );
		 tn_max = left ? tn : tn_max;
		 tn_min = left ? tn_min : tn;

         // update tn by using current inteval midpoint
         tn_next = T(0.5)*tn_max + T(0.5)*tn_min;
      }

      tn = tn_next ;
      num_iters++ ;

      // exit when the max number of iterations is exceeded
      if ( 20 < num_iters   )
      {
         break ;
      }
   }

   // done
   return tn ;
}

// ---------------------------------------------------------------------------

T eval_Ar_inverse( PSCMaps map, T Ar_value )
{
   const T Ar_max_value = T(0.5)*map.F ;

   Ar_value = max( T(0.0), min( Ar_value, Ar_max_value ));

   // for the ellipse only case, just do analytical inversion of the integral
   // (this in fact is never used as we do sampling in a scaled disk)
   if ( map.fully_visible )
      return eval_ArE_inverse( map, Ar_value );

   const T A_frac = Ar_value/Ar_max_value ;

   // in the ellipse+lune case, when Ar(phi_l) <= Ar_value, (result angle above phi_l)
   // we can and must do analytical inversion (for efficiency and convergence)
   if ( ! map.center_below_hor )
   if ( map.AE_phi_l + map.L < Ar_value )
      return eval_ArE_inverse( map, Ar_value - map.L ) ;


   // in the lune only case, for small values of L, use a parabola approximation
   if ( map.center_below_hor && map.L < 1.0e-5f )         // small lune area
   {
      return map.phi_l*( T(1.0)-sqrt( T(1.0)-A_frac ) );  // inverse parabola
   }

   // -------
   // cases involving the lune: either lune only or ellipse+lune and Ar_value <= Ar_phi_l
   // do numerical iterative inversion:

   const T theta_max    = map.center_below_hor ? map.phi_l : T(3.14159265f);
   const T theta_result = InverseNSB( map, theta_max, A_frac, T(1.0));
   const T result = max( T(0.0), min( theta_result, theta_max ));

   return result ;
}

// ---------------------------------------------------------------------------
// radial map: computes (x,y) from (s,t)
void rad_map( PSCMaps map, T s, T t, out T x, out T y )
{
   // compute 'u' by scaling and translating 't'
   const bool  angle_is_neg = t < T(0.5)   ;
   const float u            = angle_is_neg ? T(1.0)-T(2.0)*t
                                           : T(2.0)*t - T(1.0) ;

   // compute varphi in [0,1] from U by using inverse of Er, Lr or Ur

   T varphi, rmin, rmax ;
   bool scaled = false ;

   if ( map.fully_visible )
   {
      //varphi = eval_Er_inv( u*E );  // ellipse only
      varphi = 3.14159265f*u ; // as we are in 'scaled' coord. space, this is simple...
      rmin = T(0.0);
      rmax = T(1.0);
      scaled = true ;
   }
   else // partially visible
   {
      varphi = max( T(0.0), min( T(3.14159265f),
                  eval_Ar_inverse( map, u*T(0.5)*map.F  ) ));
      eval_rmin_rmax( map, varphi, rmin, rmax );
   }

   // compute x' and y'

   const T si  = angle_is_neg ? -(sin(varphi)) : sin( varphi );
   const T co  = sqrt( T(1.0) - si*si )
                     * ( varphi <= T(3.14159265f)*T(0.5) ? T(1.0) : T(-1.0) ); // note sign correction
   const T rad = sqrt( s*(rmax*rmax) + (T(1.0)-s)*(rmin*rmin)  );
   const T xp  = rad*co ;
   const T yp  = rad*si ;

   // compute x and y
   if ( scaled )
   {
      x = map.xe + map.ax*xp ;
      y = map.ay*yp ;
   }
   else
   {
      x = map.xe + xp ;
      y = yp ;
   }
}

// --------------------------------------------------------------------------
// initializes the maps object

PSCMaps initializePSCMap( const T p_alpha, const T p_beta )
{
   T tolerance = 1.0e-5f, pi2 = T(3.14159265f)*T(0.5);

   PSCMaps result;

   const T alpha = max( T(0.0), min( pi2, p_alpha ) ),
           beta  = max( -pi2,   min( pi2, p_beta  ) );

   result.E = 0.0 ;
   result.L = 0.0 ;
   result.F = 0.0 ;

   result.ay           = sin( alpha );
   result.ay_sq        = result.ay*result.ay ;
   result.r1maysq      = sqrt( T(1.0)-result.ay_sq ) ;
   result.sin_beta     = sin( beta ),
   result.sin_beta_abs = abs( result.sin_beta );
   result.cos_beta_sq  = T(1.0)- result.sin_beta*result.sin_beta ;
   result.cos_beta     = sqrt( result.cos_beta_sq );  // spherical cap center, X coord.
   result.xe           = result.cos_beta*result.r1maysq ;     // ellipse center
   result.ax           = result.ay*result.sin_beta_abs ;     // semi-minor axis length (UNSIGNED)
   result.axay2        = result.ax*result.ay*T(0.5);
   result.xe_sq        = result.xe*result.xe ;

   // intialize boolean values
   result.fully_visible     = false ;
   result.partially_visible = false ;
   result.invisible         = false ;
   result.center_below_hor  = false ;

   if ( result.ay <= result.sin_beta )      result.fully_visible     = true ;
   else if ( -result.ay < result.sin_beta ) result.partially_visible = true ;
   else	                                    result.invisible         = true ;

   result.center_below_hor = ( result.sin_beta < T(0.0) );
   
   // Initialize some values with bogus values to make the compiler happy
   result.AE_phi_l = 0.0 ;
   result.xl = 0.0 ;
   result.yl  = 0.0;
   result.phi_l    = 0.0;

   // if the sphere is not visible, mark is initialized, and do an early exit
   if ( result.invisible )
   {
      return result;
   }

   result.AE_phi_l = 0.0 ;

   if ( result.partially_visible )
   {
      result.xl = result.r1maysq/result.cos_beta ;
      result.yl  = sqrt(T(1.0)-result.xl*result.xl);

      // compute L
      result.phi_l    = atan2( result.yl, result.xl-result.xe );
      result.AE_phi_l = eval_ArE( result, result.phi_l );
      result.L        = eval_ArC( result, result.phi_l ) - result.AE_phi_l ;

      if ( result.L < T(0.0) )
         result.L = T(0.0) ;

      // compute E
      if ( ! result.center_below_hor ) // ellipse only or ellipse+lune cases (both maps)
         result.E = T(3.14159265f)*result.axay2 ;  // half ellipse area
   }
   else
      result.E = T(3.14159265f)*result.axay2 ;  // half ellipse area

   // compute F
   result.F = T(2.0)*(result.E+result.L) ;

   return result;
}