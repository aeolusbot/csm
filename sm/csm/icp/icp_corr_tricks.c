#include <gsl/gsl_vector.h>

#include "../csm_all.h"

#define DEBUG_SEARCH(a) ;


void ld_create_jump_tables(struct laser_data* ld) {
	int i;
	for(i=0;i<ld->nrays;i++) {
		int j;
		
		j=i+1;
		while(j<ld->nrays && ld->valid[j] && ld->readings[j]<=ld->readings[i]) j++;
		ld->up_bigger[i] = j-i;

		j = i+1;
		while(j<ld->nrays && ld->valid[j] && ld->readings[j]>=ld->readings[i]) j++;
		ld->up_smaller[i] = j-i;
		
		j = i-1;
		while(j>=0 && ld->valid[j] && ld->readings[j]>=ld->readings[i]) j--;
		ld->down_smaller[i] = j-i;

		j = i-1;
		while(j>=0 && ld->valid[j] && ld->readings[j]<=ld->readings[i]) j--;
		ld->down_bigger[i] = j-i;
	}	
}

extern int distance_counter;

#define INLINE inline

INLINE double local_distance_squared_d(const double* a, const double* b)  {
	distance_counter++;
	double x = a[0] - b[0];
	double y = a[1] - b[1];
	return x*x + y*y;
}

/** This is an approximation to sin(x), such that 
    mysin(x) >= sin(x) for 0 < x < 2 */
INLINE double mysin2(double x) {
	if(x < 0 || x > 2.5){
		static int once = 1;
		if(once) sm_error("mysin(): Requested x = %f\n", x);
		once = 0;
		return sin(x);
	}
	
	/* taylor: x - (1/6)*x^3 + (1/120)*x^5 + o(x^7) */
	const double a = 1.0/6.0;
	const double b = -1.0/120.0;
	double x2 = x*x;
	return x * (1 + x2 * ( a + b * x2));
}

INLINE double mysin(double x) {
	if(x > 2) return sin(x);
	const double a = 1.0/6.0;
	const double b = -1.0/120.0;
	double x2 = x*x;
	return x * (.99 + x2 * ( a + b * x2));
}

#define SQUARE(a) ((a)*(a))

void find_correspondences_tricks(struct sm_params*params, gsl_vector* x_old) {
	LDP laser_ref  = params->laser_ref;
	LDP laser_sens = params->laser_sens;
	
	/* First of all, compute points in laser_ref reference;
	   we will also need the polar coordinates */
	struct {
		double p_w[2];
		double rho, phi;
	} points[laser_sens->nrays];
	
	double pose_x = gvg(x_old, 0);
	double pose_y = gvg(x_old, 1);
	double pose_theta = gvg(x_old, 2);
	double cos_theta = cos(pose_theta); 
	double sin_theta = sin(pose_theta);
	const int nrays = laser_sens->nrays ;
	int i; for(i=0;i<nrays;i++) {
		double *p_i = laser_sens->p[i]->data;
		double x = cos_theta * p_i[0] -sin_theta*p_i[1] + pose_x;
		double y = sin_theta * p_i[0] +cos_theta*p_i[1] + pose_y;
		/* polar coordinates */
		points[i].rho = sqrt(x*x+y*y);
		points[i].phi = atan2(y,x);
		points[i].p_w[0] = x;
		points[i].p_w[1] = y;
	}
	
	/* Handy constants */
/*	double C0 = M_PI/laser_ref->nrays;*/
	double C1 =  (double)laser_ref->nrays / (laser_ref->max_theta-laser_ref->min_theta) ;
	double max_correspondence_dist2 = square(params->max_correspondence_dist);
	/* Last match */
	int last_best = -1;
	for(i=0;i<laser_sens->nrays;i++) {
		if(!ld_valid_ray(laser_sens,i)) {
			ld_set_null_correspondence(laser_sens,i);
			continue; 
		}
		
		double *p_i_w = points[i].p_w;
		double p_i_w_nrm2 = points[i].rho;
		double p_i_w_phi = points[i].phi;
		
		/** Search domain for j1 */
		int from = 0; 
		int to = laser_ref->nrays-1; 
/*		int start_cell = (int) ((p_i_w_phi - laser_ref->min_theta) * C1); */

		/** Current best match */
		int j1 = -1;
		/** and his distance */
		double best_dist = 42;
		
		/** If last match was succesful, then start at that index + 1 */
		int we_start_at; 
		if (last_best==-1) {
			we_start_at = (int) ((p_i_w_phi - laser_ref->min_theta) * C1); 
	 	} else {
			we_start_at = last_best + 1;
		}
		if(we_start_at > to) we_start_at = to;
		if(we_start_at < from) we_start_at = from;
		
		int up =  we_start_at+1; 
		int down = we_start_at; 
		double last_dist_up = 0; /* first is down */
		double last_dist_down = -1;	

		int up_stopped = 0; 
		int down_stopped = 0;
	
		DEBUG_SEARCH(printf("i=%d p_i_w = %f %f\n",i, p_i_w[0], p_i_w,[1]));
		DEBUG_SEARCH(printf("i=%d [from %d down %d mid %d up %d to %d]\n",
			i,from,down,start_cell,up,to));
		
		
		while ( (!up_stopped) || (!down_stopped) ) {
			int now_up = up_stopped ? 0 : 
			           down_stopped ? 1 : last_dist_up < last_dist_down;
			DEBUG_SEARCH(printf("|"));

			/* Now two symmetric chunks of code, the now_up and the !now_up */
			if(now_up) {
				DEBUG_SEARCH(printf("up %d ",up));
				/* If we have crossed the "to" boundary we stop searching
					on the "up" direction. */
				if(up > to) { up_stopped = 1; continue; }
				/* Just ignore invalid rays */
				if(!laser_ref->valid[up]) { ++up; continue; }
				
				/* This is the distance from p_i_w to the "up" point*/
				last_dist_up = local_distance_squared_d(p_i_w, laser_ref->p[up]->data);
				
				/* If it is less than the best point, it is our new j1 */
				if( (last_dist_up<max_correspondence_dist2) && 
					((j1==-1)||(last_dist_up < best_dist))) {
						j1 = up; best_dist = last_dist_up;
				}
				
				double delta_theta = (laser_ref->theta[up] - p_i_w_phi);
				/* If we are moving away from start_cell */
				if (delta_theta > 0) {
					/* We can compute a bound for early stopping. Currently
					   our best point has distance best_dist; we can compute
					   min_dist_up, which is the minimum distance that can have
					   points for j>up (see figure)*/
					/*double delta_theta = (up-start_cell) * C0; /*C0 = (M_PI/laser_ref->nrays);*/
					double min_dist_up = sin(delta_theta) * p_i_w_nrm2;
					/* If going up we can't make better than best_dist, then
					    we stop searching in the "up" direction */
					if(SQUARE(min_dist_up) > best_dist) { 
						up_stopped = 1; continue;
					}
					/* If we are moving away, then we can implement the jump tables
					   optimizations. */
					up += 
						/* If p_i_w is shorter than "up" */
						(laser_ref->readings[up] < p_i_w_nrm2) 
						?
						/* We can jump to a bigger point */
						laser_ref->up_bigger[up] 
						/* Or else we jump to a smaller point */ 
						: laser_ref->up_smaller[up];
						
				} else 
					/* If we are moving towards "start_cell", we can't do any
					   ot the previous optimizations and we just move to the next point */
					++up;
				
			}
			
			/* This is the specular part of the previous chunk of code. */
			if(!now_up) {
				DEBUG_SEARCH(printf("down %d ",down));
				if(down < from) { down_stopped = 1; continue; }
				if(!laser_ref->valid[down]) { --down; continue; }
		
				last_dist_down = local_distance_squared_d(p_i_w, laser_ref->p[down]->data);
				if( (last_dist_down<max_correspondence_dist2) && 
				    ((j1==-1)||(last_dist_down < best_dist))) {
						j1 = down; best_dist = last_dist_down;
				}

				double delta_theta = (p_i_w_phi - laser_ref->theta[down]);
/*				if (down < start_cell) {*/
				if(laser_ref->theta[down] + M_PI/180 < p_i_w_phi) {
				/*	double min_dist_down = table[start_cell-down] * p_i_w_nrm2;*/
				/*	double delta_theta = (start_cell-down) * C0; /* C0 = (M_PI/laser_ref->nrays) */
					double min_dist_down = sin(delta_theta) * p_i_w_nrm2;
					if( SQUARE(min_dist_down) > best_dist) { 
						down_stopped = 1; continue;
					}
					down += (laser_ref->readings[down] < p_i_w_nrm2) ?
						laser_ref->down_bigger[down] : laser_ref->down_smaller[down];
				} else --down;
			}
			
		}
		
		DEBUG_SEARCH(printf("i=%d j1=%d dist=%f\n",i,j1,best_dist));
		
		/* If no point matched. */
		if(-1==j1) {
			ld_set_null_correspondence(laser_sens, i);
			continue;
		}
		/* We ignore matching the first or the last point in the scan */
		if( 0==j1 || j1 == (laser_ref->nrays-1)) {/* no match */
			ld_set_null_correspondence(laser_sens, i);
			continue;
		}

		/* Now we want to find j2, the second best match. */
		int j2;
		/* We find the next valid point, up and down */
		int j2up   = ld_next_valid_up   (laser_ref, j1);
		int j2down = ld_next_valid_down (laser_ref, j1);
		/* And then (very boring) we use the nearest */
		if((j2up==-1)&&(j2down==-1)) {
			ld_set_null_correspondence(laser_sens, i);
			continue;
		}
		if(j2up  ==-1) { j2 = j2down; } else
		if(j2down==-1) { j2 = j2up; } else {
			double dist_up   = local_distance_squared_d(p_i_w, laser_ref->p[j2up  ]->data);
			double dist_down = local_distance_squared_d(p_i_w, laser_ref->p[j2down]->data);
			j2 = dist_up < dist_down ? j2up : j2down;
		}

		last_best = j1;
		
		laser_sens->corr[i].valid = 1;
		laser_sens->corr[i].j1 = j1;
		laser_sens->corr[i].j2 = j2;
		laser_sens->corr[i].dist2_j1 = best_dist;
		
	}

}


