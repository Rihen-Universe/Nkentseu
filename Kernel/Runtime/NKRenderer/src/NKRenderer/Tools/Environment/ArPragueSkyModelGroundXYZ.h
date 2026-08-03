#ifndef _ARPRAGUESKYMODELGROUND_H_
#define _ARPRAGUESKYMODELGROUND_H_

/*
Prague Sky Model, ground level XYZ version, 2.6.2021

Provides sky radiance for rays going from the ground into the upper hemisphere. 

Sky appearance is parametrized by:
- elevation = solar elevation in radians (angle between diretion to sun and ground plane), supported values in range [-0.073304, 1.570796] (corrsponds to [-4.2, 90] degrees)
- visibility = meteorological range in km (how far one can see), supported values in range [20, 131.8] (corresponds to turbity range [3.7, 1.37])
- albedo = ground albedo, supported values in range [0, 1]

Usage:
1. First call arpragueskymodelground_state_alloc_init to get initialized model state
2. Then to compute sky radiance use the model state when calling arpragueskymodelground_sky_radiance
3. Finally call arpragueskymodelground_state_free to free used memory

Model query parameters:
- theta = angle between view direction and direction to zenith in radians, supported values in range [0, PI]
- gamma = angle between view direction and direction to sun in radians, supported values in range [0, PI]
- shadow = angle between view direction and direction perpendicular to a shadow plane (= direction to sun rotated PI/2 towards direction to zenith) in radians, used for negative solar elevations only, supported values in range [0, PI]
- channel = index of XYZ channel, supported values are 0 (=X), 1 (=Y), 2(=Z)
- distance = length of a ray segment (going from view point along view direction) for which transmittance should be evaluated, supported values in range [0, +inf]

Differences to Hosek model:
- uses visibility instead of turbidity (but visibility can be computed from turbidity as: visibility = 7487.f * exp(-3.41f * turbidity) + 117.1f * exp(-0.4768f * turbidity))
- supports negative solar elevations but for that requires additional parameter, the shadow angle (can be computed using the arpragueskymodelground_compute_angles function, unused for nonnegative solar elevations)
*/

#ifndef MATH_PI
#define MATH_PI                    3.141592653589793
#endif

#ifndef MATH_RAD_TO_DEG
#define MATH_RAD_TO_DEG            ( 180.0 / MATH_PI )
#endif

#ifndef MATH_DEG_TO_RAD
#define MATH_DEG_TO_RAD            ( MATH_PI / 180.0)
#endif

#define PSMG_SUN_RADIUS             0.2667 * MATH_DEG_TO_RAD
#define PSMG_PLANET_RADIUS          6378000.0
#define PSMG_PLANET_RADIUS_SQR      PSMG_PLANET_RADIUS * PSMG_PLANET_RADIUS
#define PSMG_ATMO_WIDTH             100000.0

typedef struct ArPragueSkyModelGroundState
{
	// Radiance metadata

	int visibilities;
	double * visibility_vals;

	int albedos;
	double * albedo_vals;
	
	int altitudes;
	double * altitude_vals;

	int elevations;
	double * elevation_vals;

	int channels;

	int tensor_components;

	int sun_nbreaks;
	int sun_offset;
	int sun_stride;
	double * sun_breaks;

	int zenith_nbreaks;
	int zenith_offset;
	int zenith_stride;
	double * zenith_breaks;

	int emph_nbreaks;
	int emph_offset;
	double * emph_breaks;

	int total_coefs_single_config;
	int total_coefs_all_configs;
	int total_configs;

	// Radiance data

	double * radiance_dataset;
	
	// Configuration
	
	double elevation;
	double visibility;
    double albedo;
}
ArPragueSkyModelGroundState;

// Initializes state of the model and returns it. Must be called before calling other functions. Expects full path to the file with model dataset.
ArPragueSkyModelGroundState  * arpragueskymodelground_state_alloc_init(
	const char                   * path_to_dataset,
	const double                   elevation,
	const double                   visibility,
	const double                   albedo
	);

// Free memory used by the model.
void arpragueskymodelground_state_free(
	ArPragueSkyModelGroundState        * state
	);

// Helper function that computes angles required by the model given the current configuration. Expects:
// - solar elevation at view point in radians
// - solar azimuth at view point in radians
// - view direction as an array of 3 doubles
// - direction to zenith as an array of 3 doubles (e.g. {0.0, 0.0, 1.0} or an actual direction to zenith based on true view point position on the planet)
void arpragueskymodelground_compute_angles(
	const double		           sun_elevation,
	const double		           sun_azimuth,
	const double		         * view_direction,
	const double		         * up_direction,
		  double                 * theta,
		  double                 * gamma,
		  double                 * shadow
	);

// Computes sky radiance arriving at view point.
double arpragueskymodelground_sky_radiance(
	const ArPragueSkyModelGroundState  * state,
	const double                   theta,
	const double                   gamma,
	const double                   shadow,
	const int                      channel
	);

#endif // _ARPRAGUESKYMODELGROUND_H_
