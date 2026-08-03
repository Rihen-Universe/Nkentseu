#include "ArPragueSkyModelGroundXYZ.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

//   Some macro definitions that occur elsewhere in ART, and that have to be
//   replicated to make this a stand-alone module.

#ifndef ALLOC
#define ALLOC(_struct)                ((_struct *)malloc(sizeof(_struct)))
#endif

#ifndef ALLOC_ARRAY
#define ALLOC_ARRAY(_struct, _number) ((_struct *)malloc(sizeof(_struct) * (_number)))
#endif

#ifndef FREE
#define FREE(_pointer) \
do { \
    void *_ptr=(void *)(_pointer); \
    free(_ptr); \
    _ptr=NULL; \
    _pointer=NULL; \
} while (0)
#endif

#ifndef MATH_MAX
#define MATH_MAX(_a, _b)                 ((_a) > (_b) ? (_a) : (_b))
#endif

#ifndef MATH_HUGE_DOUBLE
#define MATH_HUGE_DOUBLE        5.78960446186580977117855E+76
#endif

double arpragueskymodelground_double_from_half(const unsigned short value)
{
	unsigned long hi = (unsigned long)(value&0x8000) << 16;
	unsigned int abs = value & 0x7FFF;
	if(abs)
	{
		hi |= 0x3F000000 << (unsigned)(abs>=0x7C00);
		for(; abs<0x400; abs<<=1,hi-=0x100000) ;
		hi += (unsigned long)(abs) << 10;
	}
	unsigned long dbits = (unsigned long)(hi) << 32;
	double out;
	memcpy(&out, &dbits, sizeof(double));
	return out;
}

int arpragueskymodelground_compute_pp_coefs_from_half(const int nbreaks, const double * breaks, const unsigned short * values, double * coefs, const int offset, const double scale)
{
	for (int i = 0; i < nbreaks - 1; ++i)
	{
		const double val1 = arpragueskymodelground_double_from_half(values[i+1]) / scale;
		const double val2 = arpragueskymodelground_double_from_half(values[i]) / scale;
		const double diff = val1 - val2;

		coefs[offset + 2 * i] = diff / (breaks[i+1] - breaks[i]);
		coefs[offset + 2 * i + 1]  = val2;
	}
	return 2 * nbreaks - 2;
}

void arpragueskymodelground_print_error_and_exit(const char * message) 
{
	fprintf(stderr, message);
	fprintf(stderr, "\n");
	fflush(stderr);
	exit(-1);
}

void arpragueskymodelground_print_items(int val, const char * name) {
    return;
    printf("%d %s:\n", val, name);
}

void arpragueskymodelground_print_double_iter(int count, const double * arr) {
    return;
    int i = 0;
	for (i = 0; i < count; ++i)
	{
		printf("  %f\n", arr[i]);
	}
}

void arpragueskymodelground_read_radiance(ArPragueSkyModelGroundState * state, FILE * handle)
{
	// Read metadata

	// Structure of the metadata part of the data file:
	// visibilities      (1 * int),  visibility_vals (visibilities * double),
	// albedos           (1 * int),  albedo_vals    (albedos * double),
	// altitudes         (1 * int),  altitude_vals  (altitudes * double),
	// elevations        (1 * int),  elevation_vals (elevations * double),
	// channels          (1 * int),
	// tensor_components (1 * int),
    // sun_nbreaks       (1 * int),  sun_breaks     (sun_nbreaks * double),
	// zenith_nbreaks    (1 * int),  zenith_breaks  (zenith_nbreaks * double),
	// emph_nbreaks      (1 * int),  emph_breaks    (emph_nbreaks * double)

	int vals_read;

	vals_read = fread(&state->visibilities, sizeof(int), 1, handle);
	if (vals_read != 1 || state->visibilities < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: visibilities");
    
    arpragueskymodelground_print_items(state->visibilities, "visibilities");

	state->visibility_vals = ALLOC_ARRAY(double, state->visibilities);
	vals_read = fread(state->visibility_vals, sizeof(double), state->visibilities, handle);
	if (vals_read != state->visibilities) arpragueskymodelground_print_error_and_exit("Error reading sky model data: visibility_vals");
    
    arpragueskymodelground_print_double_iter(state->visibilities, state->visibility_vals);

	vals_read = fread(&state->albedos, sizeof(int), 1, handle);
	if (vals_read != 1 || state->albedos < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: albedos");
    
    arpragueskymodelground_print_items(state->albedos, "albedos");

	state->albedo_vals = ALLOC_ARRAY(double, state->albedos);
	vals_read = fread(state->albedo_vals, sizeof(double), state->albedos, handle);
	if (vals_read != state->albedos) arpragueskymodelground_print_error_and_exit("Error reading sky model data: albedo_vals");

    arpragueskymodelground_print_double_iter(state->albedos, state->albedo_vals);
    
	vals_read = fread(&state->altitudes, sizeof(int), 1, handle);
	if (vals_read != 1 || state->altitudes < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: altitudes");
    
    arpragueskymodelground_print_items(state->altitudes, "altitudes");

	state->altitude_vals = ALLOC_ARRAY(double, state->altitudes);
	vals_read = fread(state->altitude_vals, sizeof(double), state->altitudes, handle);
	if (vals_read != state->altitudes) arpragueskymodelground_print_error_and_exit("Error reading sky model data: altitude_vals");

    arpragueskymodelground_print_double_iter(state->altitudes, state->altitude_vals);
    
	vals_read = fread(&state->elevations, sizeof(int), 1, handle);
	if (vals_read != 1 || state->elevations < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: elevations");
    
    arpragueskymodelground_print_items(state->elevations, "elevations");

	state->elevation_vals = ALLOC_ARRAY(double, state->elevations);
	vals_read = fread(state->elevation_vals, sizeof(double), state->elevations, handle);
	if (vals_read != state->elevations) arpragueskymodelground_print_error_and_exit("Error reading sky model data: elevation_vals");
    
    arpragueskymodelground_print_double_iter(state->elevations, state->elevation_vals);


	vals_read = fread(&state->channels, sizeof(int), 1, handle);
	if (vals_read != 1 || state->channels < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: channels");
    
    arpragueskymodelground_print_items(state->channels, "channels");

	vals_read = fread(&state->tensor_components, sizeof(int), 1, handle);
	if (vals_read != 1 || state->tensor_components < 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: tensor_components");
    
    arpragueskymodelground_print_items(state->tensor_components, "tensor_components");

	vals_read = fread(&state->sun_nbreaks, sizeof(int), 1, handle);
	if (vals_read != 1 || state->sun_nbreaks < 2) arpragueskymodelground_print_error_and_exit("Error reading sky model data: sun_nbreaks");
    
    arpragueskymodelground_print_items(state->sun_nbreaks, "sun_nbreaks");

	state->sun_breaks = ALLOC_ARRAY(double, state->sun_nbreaks);
	vals_read = fread(state->sun_breaks, sizeof(double), state->sun_nbreaks, handle);
	if (vals_read != state->sun_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: sun_breaks");

    arpragueskymodelground_print_double_iter(state->sun_nbreaks, state->sun_breaks);
    
	vals_read = fread(&state->zenith_nbreaks, sizeof(int), 1, handle);
	if (vals_read != 1 || state->zenith_nbreaks < 2) arpragueskymodelground_print_error_and_exit("Error reading sky model data: zenith_nbreaks");
    
    arpragueskymodelground_print_items(state->zenith_nbreaks, "zenith_nbreaks");

	state->zenith_breaks = ALLOC_ARRAY(double, state->zenith_nbreaks);
	vals_read = fread(state->zenith_breaks, sizeof(double), state->zenith_nbreaks, handle);
	if (vals_read != state->zenith_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: zenith_breaks");
    
    arpragueskymodelground_print_double_iter(state->zenith_nbreaks, state->zenith_breaks);

	vals_read = fread(&state->emph_nbreaks, sizeof(int), 1, handle);
	if (vals_read != 1 || state->emph_nbreaks < 2) arpragueskymodelground_print_error_and_exit("Error reading sky model data: emph_nbreaks");
    
    arpragueskymodelground_print_items(state->emph_nbreaks, "emph_nbreaks");

	state->emph_breaks = ALLOC_ARRAY(double, state->emph_nbreaks);
	vals_read = fread(state->emph_breaks, sizeof(double), state->emph_nbreaks, handle);
	if (vals_read != state->emph_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: emph_breaks");
    
    arpragueskymodelground_print_double_iter(state->emph_nbreaks, state->emph_breaks);


	// Calculate offsets and strides

	state->sun_offset = 0;
	state->sun_stride = 2 * state->sun_nbreaks - 2 + 2 * state->zenith_nbreaks - 2;

	state->zenith_offset = state->sun_offset + 2 * state->sun_nbreaks - 2;
	state->zenith_stride = state->sun_stride;

	state->emph_offset = state->sun_offset + state->tensor_components * state->sun_stride;

	state->total_coefs_single_config = state->emph_offset + 2 * state->emph_nbreaks - 2;
	state->total_configs = state->channels * state->elevations * state->altitudes * state->albedos * state->visibilities;
	state->total_coefs_all_configs = state->total_coefs_single_config * state->total_configs;

	// Read data

	// Structure of the data part of the data file:
	// [[[[[[ sun_coefs (sun_nbreaks * half), zenith_scale (1 * double), zenith_coefs (zenith_nbreaks * half) ] * tensor_components, emph_coefs (emph_nbreaks * half) ]
	//   * channels ] * elevations ] * altitudes ] * albedos ] * visibilities

	int offset = 0;
	state->radiance_dataset = ALLOC_ARRAY(double, state->total_coefs_all_configs);

	unsigned short * radiance_temp = ALLOC_ARRAY(unsigned short, MATH_MAX(state->sun_nbreaks, MATH_MAX(state->zenith_nbreaks, state->emph_nbreaks)));

	for (int con = 0; con < state->total_configs; ++con)
	{
		for (int tc = 0; tc < state->tensor_components; ++tc)
		{
			const double sun_scale = 1.0;
			vals_read = fread(radiance_temp, sizeof(unsigned short), state->sun_nbreaks, handle);
			if (vals_read != state->sun_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: sun_coefs");
			offset += arpragueskymodelground_compute_pp_coefs_from_half(state->sun_nbreaks, state->sun_breaks, radiance_temp, state->radiance_dataset, offset, sun_scale);

			double zenith_scale;
			vals_read = fread(&zenith_scale, sizeof(double), 1, handle);
			if (vals_read != 1) arpragueskymodelground_print_error_and_exit("Error reading sky model data: zenith_scale");

			vals_read = fread(radiance_temp, sizeof(unsigned short), state->zenith_nbreaks, handle);
			if (vals_read != state->zenith_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: zenith_coefs");
			offset += arpragueskymodelground_compute_pp_coefs_from_half(state->zenith_nbreaks, state->zenith_breaks, radiance_temp, state->radiance_dataset, offset, zenith_scale);
		}

		const double emph_scale = 1.0;
		vals_read = fread(radiance_temp, sizeof(unsigned short), state->emph_nbreaks, handle);
		if (vals_read != state->emph_nbreaks) arpragueskymodelground_print_error_and_exit("Error reading sky model data: emph_coefs");
		offset += arpragueskymodelground_compute_pp_coefs_from_half(state->emph_nbreaks, state->emph_breaks, radiance_temp, state->radiance_dataset, offset, emph_scale);
	}

	free(radiance_temp);
}

ArPragueSkyModelGroundState  * arpragueskymodelground_state_alloc_init(
	const char                   * path_to_dataset,
	const double                   elevation,
	const double                   visibility,
	const double                   albedo
	)
{
	ArPragueSkyModelGroundState * state = ALLOC(ArPragueSkyModelGroundState);

	FILE * handle = fopen(path_to_dataset, "rb");

	// Read data
	arpragueskymodelground_read_radiance(state, handle);

	fclose(handle);
	
	state->elevation  = elevation;
	state->visibility = visibility;
	state->albedo     = albedo;

	return state;
}

void arpragueskymodelground_state_free(
	ArPragueSkyModelGroundState  * state
	)
{
	free(state->visibility_vals);
	free(state->albedo_vals);
	free(state->altitude_vals);
	free(state->elevation_vals);

	free(state->sun_breaks);
	free(state->zenith_breaks);
	free(state->emph_breaks);
	free(state->radiance_dataset);

	FREE(state);
}

void arpragueskymodelground_compute_angles(
	const double		           sun_elevation,
	const double		           sun_azimuth,
	const double		         * view_direction,
	const double		         * up_direction,
		  double                 * theta,
		  double                 * gamma,
		  double                 * shadow
        )
{
    // Zenith angle (theta)

    const double cosTheta = view_direction[0] * up_direction[0] + view_direction[1] * up_direction[1] + view_direction[2] * up_direction[2];
    *theta = acos(cosTheta);

    // Sun angle (gamma)

	const double sun_direction[] = {cos(sun_azimuth) * cos(sun_elevation), sin(sun_azimuth) * cos(sun_elevation), sin(sun_elevation)};
	const double cosGamma = view_direction[0] * sun_direction[0] + view_direction[1] * sun_direction[1] + view_direction[2] * sun_direction[2];
    *gamma = acos(cosGamma);

    // Shadow angle

    const double shadow_angle = sun_elevation + MATH_PI * 0.5;
	const double shadow_direction[] = {cos(shadow_angle) * cos(sun_azimuth), cos(shadow_angle) * sin(sun_azimuth), sin(shadow_angle)};
	const double cosShadow = view_direction[0] * shadow_direction[0] + view_direction[1] * shadow_direction[1] + view_direction[2] * shadow_direction[2];
    *shadow = acos(cosShadow);
}

double arpragueskymodelground_lerp(const double from, const double to, const double factor)
{
	return (1.0 - factor) * from + factor * to;
}

int arpragueskymodelground_find_segment(const double x, const int nbreaks, const double* breaks)
{
	int segment = 0;
	for (segment = 0; segment < nbreaks; ++segment)
	{
		if (breaks[segment+1] >= x)
		break;
	}
	return segment;
}

double arpragueskymodelground_eval_pp(const double x, const int segment, const double * breaks, const double * coefs)
{
	const double x0 = x - breaks[segment];
	const double * sc = coefs + 2 * segment; // segment coefs
	return sc[0] * x0 + sc[1];
}

const double * arpragueskymodelground_control_params_single_config(
	const ArPragueSkyModelGroundState * state,
	const double                * dataset,
	const int                     total_coefs_single_config,
	const int                     elevation,
	const int                     altitude,
	const int                     visibility,
	const int                     albedo,
	const int                     channel
)
{
	return dataset + (total_coefs_single_config * (
		channel +
		state->channels*elevation +
		state->channels*state->elevations*altitude +
		state->channels*state->elevations*state->altitudes*albedo +
		state->channels*state->elevations*state->altitudes*state->albedos*visibility
	));
}

double arpragueskymodelground_reconstruct(
	const ArPragueSkyModelGroundState  * state,
	const double                   gamma,
	const double                   alpha,
	const double                   theta,
	const int                      gamma_segment,
	const int                      alpha_segment,
	const int                      theta_segment,
	const double                 * control_params
)
{
  double res = 0.0;
  for (int t = 0; t < state->tensor_components; ++t) {
	const double sun_val_t = arpragueskymodelground_eval_pp(gamma, gamma_segment, state->sun_breaks, control_params + state->sun_offset + t * state->sun_stride);
	const double zenith_val_t = arpragueskymodelground_eval_pp(alpha, alpha_segment, state->zenith_breaks, control_params + state->zenith_offset + t * state->zenith_stride);
	res += sun_val_t * zenith_val_t;
  }
  const double emph_val_t = arpragueskymodelground_eval_pp(theta, theta_segment, state->emph_breaks, control_params + state->emph_offset);
  res *= emph_val_t;

  return MATH_MAX(res, 0.0);
}

double arpragueskymodelground_map_parameter(const double param, const int value_count, const double * values)
{
	double mapped;
	if (param < values[0])
	{
		mapped = 0.0;
	}
	else if (param > values[value_count - 1])
	{
		mapped = (double)value_count - 1.0;
	}
	else
	{
		for (int v = 0; v < value_count; ++v)
		{
			const double val = values[v];
			if (fabs(val - param) < 1e-6)
			{
				mapped = v;
				break;
			}
			else if (param < val)
			{
				mapped = v - ((val - param) / (val - values[v - 1]));
				break;
			}
		}
	}
	return mapped;
}


///////////////////////////////////////////////
// Sky radiance
///////////////////////////////////////////////


double arpragueskymodelground_interpolate_elevation(
	const ArPragueSkyModelGroundState  * state,
	double                  elevation,
	int                     altitude,
	int                     visibility,
	int                     albedo,
	int                     channel,
	double                  gamma,
	double                  alpha,
	double                  theta,
	int                     gamma_segment,
	int                     alpha_segment,
	int                     theta_segment
)
{
  const int elevation_low = (int)elevation;
  const double factor = elevation - (double)elevation_low;

  const double * control_params_low = arpragueskymodelground_control_params_single_config(
    state,
    state->radiance_dataset,
    state->total_coefs_single_config,
    elevation_low,
    altitude,
    visibility,
    albedo,
    channel);

  double res_low = arpragueskymodelground_reconstruct(
    state,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment,
    control_params_low);    

  if (factor < 1e-6 || elevation_low >= (state->elevations - 1))
  {
    return res_low;
  }

  const double * control_params_high = arpragueskymodelground_control_params_single_config(
    state,
    state->radiance_dataset,
    state->total_coefs_single_config,
    elevation_low+1,
    altitude,
    visibility,
    albedo,
    channel);

  double res_high = arpragueskymodelground_reconstruct(
    state,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment,
    control_params_high); 

  return arpragueskymodelground_lerp(res_low, res_high, factor);
}

double arpragueskymodelground_interpolate_altitude(
	const ArPragueSkyModelGroundState  * state,
	double                  elevation,
	double                  altitude,
	int                     visibility,
	int                     albedo,
	int                     channel,
	double                  gamma,
	double                  alpha,
	double                  theta,
	int                     gamma_segment,
	int                     alpha_segment,
	int                     theta_segment
)
{
  const int altitude_low = (int)altitude;
  const double factor = altitude - (double)altitude_low;

  double res_low = arpragueskymodelground_interpolate_elevation(
    state,
    elevation,
    altitude_low,
    visibility,
    albedo,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  if (factor < 1e-6 || altitude_low >= (state->altitudes - 1))
  {
    return res_low;
  }

  double res_high = arpragueskymodelground_interpolate_elevation(
    state,
    elevation,
    altitude_low + 1,
    visibility,
    albedo,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  return arpragueskymodelground_lerp(res_low, res_high, factor);
}

double arpragueskymodelground_interpolate_visibility(
	const ArPragueSkyModelGroundState  * state,
	double                  elevation,
	double                  altitude,
	double                  visibility,
	int                     albedo,
	int                     channel,
	double                  gamma,
	double                  alpha,
	double                  theta,
	int                     gamma_segment,
	int                     alpha_segment,
	int                     theta_segment
)
{
  const int visibility_low = (int)visibility;
  const double factor = visibility - (double)visibility_low;

  double res_low = arpragueskymodelground_interpolate_altitude(
    state,
    elevation,
    altitude,
    visibility_low,
    albedo,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  if (factor < 1e-6 || visibility_low >= (state->visibilities - 1))
  {
    return res_low;
  }

  double res_high = arpragueskymodelground_interpolate_altitude(
    state,
    elevation,
    altitude,
    visibility_low + 1,
    albedo,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  return arpragueskymodelground_lerp(res_low, res_high, factor);
}

double arpragueskymodelground_interpolate_albedo(
	const ArPragueSkyModelGroundState  * state,
	double                  elevation,
	double                  altitude,
	double                  visibility,
	double                  albedo,
	int                     channel,
	double                  gamma,
	double                  alpha,
	double                  theta,
	int                     gamma_segment,
	int                     alpha_segment,
	int                     theta_segment
)
{
  const int albedo_low = (int)albedo;
  const double factor = albedo - (double)albedo_low;

  double res_low = arpragueskymodelground_interpolate_visibility(
    state,
    elevation,
    altitude,
    visibility,
    albedo_low,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  if (factor < 1e-6 || albedo_low >= (state->albedos - 1))
  {
    return res_low;
  }

  double res_high = arpragueskymodelground_interpolate_visibility(
    state,
    elevation,
    altitude,
    visibility,
    albedo_low + 1,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  return arpragueskymodelground_lerp(res_low, res_high, factor);
}

double arpragueskymodelground_sky_radiance(
	const ArPragueSkyModelGroundState  * state,
	const double                   theta,
	const double                   gamma,
	const double                   shadow,
	const int                      channel
)
{
  // Translate parameter values to indices

  const double visibility_control = arpragueskymodelground_map_parameter(state->visibility, state->visibilities, state->visibility_vals);
  const double albedo_control     = arpragueskymodelground_map_parameter(state->albedo, state->albedos, state->albedo_vals);
  const double altitude_control   = arpragueskymodelground_map_parameter(0, state->altitudes, state->altitude_vals);
  const double elevation_control  = arpragueskymodelground_map_parameter(state->elevation * MATH_RAD_TO_DEG, state->elevations, state->elevation_vals);

  if ( channel >= state->channels || channel < 0) return 0.;

  // Get params corresponding to the indices, reconstruct result and interpolate

  const double alpha = state->elevation < 0 ? shadow : theta;

  const int gamma_segment = arpragueskymodelground_find_segment(gamma, state->sun_nbreaks, state->sun_breaks);
  const int alpha_segment = arpragueskymodelground_find_segment(alpha, state->zenith_nbreaks, state->zenith_breaks);
  const int theta_segment = arpragueskymodelground_find_segment(theta, state->emph_nbreaks, state->emph_breaks);

  const double res = arpragueskymodelground_interpolate_albedo(
    state,
    elevation_control,
    altitude_control,
    visibility_control,
    albedo_control,
    channel,
    gamma,
    alpha,
    theta,
    gamma_segment,
    alpha_segment,
    theta_segment);

  return res;
}
