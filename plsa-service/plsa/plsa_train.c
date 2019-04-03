/*
# Copyright (C) 2010 Mathieu Blondel
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <strings.h>

#define C_(cols, i, j) (i * cols + j)
#define F_(rows, i, j) (i + j * rows)
#define Z_(i,j) (C_(n_z, i, j))

#define SWAP(a,b,tmp) tmp = a; a = b; b = tmp


//int min_iteration = 30;
//int min_iteration = 120;
//int min_iteration = 10;
//int min_iteration = 6;
//double beta = 0.75;
//double beta = 1;

static void
normalize_axis0(double *a,
                unsigned int j,
                unsigned int rows,
                unsigned int cols)
{
    double sum = 0.0;
    unsigned int n;

    for (n=0; n < rows; n++)
        sum += a[C_(cols,n,j)];

    if (sum > 0.0)
    {
        for (n=0; n < rows; n++)
            a[C_(cols,n,j)] /= sum;
    }
}

static unsigned int
total_wc(unsigned int n_ele,
         unsigned int *tdnz)
{
    unsigned int n, wc=0;

    for (n=0; n < n_ele; n++)
        wc += tdnz[n];

    return wc;
}

static double
loglikelihood(unsigned int n_ele,
              unsigned int n_z,
              unsigned int *tdnz,
              double *p_z,
              double *p_w_z,
              double *p_d_z)
{
    unsigned int n, z, w, d, wc;
    double sum, L = 0.0;
    double sum111 = 0.0;

    for (n=0; n < n_ele; n++)
    {
        wc = tdnz[n];
        w = tdnz[F_(n_ele,n,1)];
        d = tdnz[F_(n_ele,n,2)];

        sum = 0.0;
        sum111 = 0.0;
        for (z=0; z < n_z; z++)
        {
           sum += p_z[z] * p_w_z[Z_(w,z)] * p_d_z[Z_(d,z)];
           sum111 += p_z[z] * p_d_z[Z_(d,z)];
        }

//        printf("Printeddddd %f %f",sum,sum111);
        sum = sum / sum111;

        if (sum > 0.0)
            L += wc * log(sum);
    }

    return L;
}

void
_train(unsigned int n_ele,
       unsigned int n_z,
       unsigned int n_w,
       unsigned int n_d,
       /*
       tdnz is a fortran-style 2d-array where:
           - 1st column: word count
           - 2nd column: word index
           - 3rd column: document index
       */
       unsigned int *tdnz,
       double *p_z,
       double *p_w_z,
       double *p_d_z,
       double *p_z_old,
       double *p_w_z_old,
       double *p_d_z_old,
       unsigned int maxiter,
       double eps,
       double beta,
       unsigned int min_iteration,
       unsigned int folding_in,
       unsigned int debug,
       double *logL_c)

{

    unsigned int iter, n, z, w, d, wc, R;
    double *p_z_d_w, *tmp, sum, lik, lik_new, lik_diff;

    R = total_wc(n_ele, tdnz);

    p_z_d_w = (double *)calloc(n_z, sizeof(double));

    lik = loglikelihood(n_ele, n_z, tdnz, p_z, p_w_z, p_d_z);

    logL_c[0] = lik;


    printf("Starting logL = %f.\n", lik);

    unsigned int max = 0;

    unsigned int iii = 0;

    if(n_z*n_w > n_z*n_d)
    { max = n_z*n_w;}
    else
    { max = n_z*n_d;}






    for (iter=0; iter < maxiter; iter++)
    {

        SWAP(p_z, p_z_old, tmp);
        SWAP(p_w_z, p_w_z_old, tmp);
        SWAP(p_d_z, p_d_z_old, tmp);

        bzero(p_z, n_z * sizeof(double));
        if (!folding_in)
        {
            bzero(p_w_z, n_w * n_z * sizeof(double));
            bzero(p_d_z, n_d * n_z * sizeof(double));
        }

        if (beta != 1)
        {

            printf("Tempering started....");

            for(iii = 0; iii < max;iii++)
            {
                if(iii < n_z)
                {
                    p_z_old[iii] = pow(p_z_old[iii],beta);
                }

                if(iii < n_z*n_w)
                {
                    p_w_z_old[iii] = pow(p_w_z_old[iii],beta);
                }

                if(iii < n_z*n_d)
                {
                    p_d_z_old[iii] = pow(p_d_z_old[iii],beta);
                }

            }


            printf("Tempering finished.\n");

        }

        for (n=0; n < n_ele; n++)
        {
            wc = tdnz[n];
            w = tdnz[F_(n_ele,n,1)];
            d = tdnz[F_(n_ele,n,2)];

            sum = 0.0;

            for (z=0; z < n_z; z++)
            {
                sum += (p_z_d_w[z] = p_z_old[z] * p_d_z_old[Z_(d,z)] *
                        p_w_z_old[Z_(w,z)]);
            }

            if (sum > 0.0)
            {
                for (z=0; z < n_z; z++)
                {
                    p_z_d_w[z] *= (wc/ sum);
                    p_d_z[Z_(d,z)] += p_z_d_w[z];
                    if (!folding_in)
                    {
                        p_w_z[Z_(w,z)] += p_z_d_w[z];
                        p_z[z] += p_z_d_w[z];
                    }
                }
            }
        } /* end for n */

        for (z=0; z < n_z; z++)
        {
            normalize_axis0(p_d_z, z, n_d, n_z);
            if (!folding_in)
            {
                normalize_axis0(p_w_z, z, n_w, n_z);
                p_z[z] /= R;
            }
        }

        lik_new = loglikelihood(n_ele, n_z, tdnz, p_z, p_w_z, p_d_z);
        //lik_diff = lik_new - lik;
        lik_diff = -(lik_new - lik)*100/lik;
        lik = lik_new;
        logL_c[iter+1] = lik;


        /*if (iter > 0 && iter % 5 == 0)
        {
            printf("."); fflush(stdout);
        }*/

        printf("Iteration %d\n",iter+1);
        printf("lik_diff = %f \n",lik_diff);

        if (iter > (min_iteration-1) && lik_diff < eps)
        {
            printf("\neps = %f \n", eps);
            break;
        }

    } /* end for iter */

    printf("\nStopped at iteration %d.\n", iter+1);


    free(p_z_d_w);
}
