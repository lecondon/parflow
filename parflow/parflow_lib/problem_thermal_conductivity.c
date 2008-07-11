/*BHEADER**********************************************************************
 * (c) 1997   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision: 1.1.1.1 $
 *********************************************************************EHEADER*/

#include "parflow.h"

/*--------------------------------------------------------------------------
 * Structures
 *--------------------------------------------------------------------------*/

typedef struct
{
   int    type; /* input type */
   void  *data; /* pointer to Type structure */

   NameArray regions;

} PublicXtra;

typedef struct
{
   Grid    *grid;

   double  *temp_data;

} InstanceXtra;

typedef struct
{
   int     num_regions;
   int    *region_indices;
   double *values;
} Type0;

typedef struct
{
   int     num_regions;
   int    *region_indices;
   int     data_from_file;
   char   *cwet_file;
   char   *cdry_file;
   double *cwets;
   double *cdrys;
   Vector *cwet_values;
   Vector *cdry_values;
} Type1;                      /* K = KRDY + Saturation * (KWET - KDRY)  */

/*--------------------------------------------------------------------------
 * Thermal Conductivity:
 *    This routine returns a Vector of thermalconductivities based on saturations.
 *--------------------------------------------------------------------------*/

void	 ThermalConductivity(phase_thermalconductivity, phase_pressure, phase_saturation, 
		    gravity, problem_data, fcn)

Vector      *phase_thermalconductivity;  /* Vector of return thermal conductivities */
Vector      *phase_pressure;    /* Vector of pressures */
Vector      *phase_saturation;     /* Vector of saturations*/
double       gravity;           /* Magnitude of gravity in neg. z direction */
ProblemData *problem_data;      /* Contaicwets geometry info. for the problem */
int          fcn;               /* Flag determining what to calculate 
                                 * fcn = CALCFCN => calculate the function
				 *                  value
                                 * fcn = CALCDER => calculate the function 
                                 *                  derivative */
{
   PFModule      *this_module   = ThisPFModule;
   PublicXtra    *public_xtra   = PFModulePublicXtra(this_module);

   Type0         *dummy0;
   Type1         *dummy1;

   Grid          *grid             = VectorGrid(phase_thermalconductivity);

   GrGeomSolid   *gr_solid, *gr_domain;

   Subvector     *pt_sub;
   Subvector     *pp_sub;
   Subvector     *ps_sub;
   Subvector     *cwet_values_sub;
   Subvector     *cdry_values_sub;

   double        *ptdat, *ppdat, *psdat;
   double        *cwet_values_dat, *cdry_values_dat;

   SubgridArray  *subgrids = GridSubgrids(grid);

   Subgrid       *subgrid;

   int            sg;

   int            ix,   iy,   iz, r;
   int            nx,   ny,   nz;

   int            i, j, k, ipt, ips, ipp, ipd, ipRF;

   int            cwet_index, cdry_index;

   int            *region_indices, num_regions, ir;

   /* Initialize thermal conductivity to 0.0 */
   InitVectorAll(phase_thermalconductivity,0.0);

   switch((public_xtra -> type))
   {

   case 0:  /* Constant thermal conductivity*/
   {
      double  *values;
      int      ir;

      dummy0 = (Type0 *)(public_xtra -> data);

      num_regions    = (dummy0 -> num_regions);
      region_indices = (dummy0 -> region_indices);
      values         = (dummy0 -> values);

      for (ir = 0; ir < num_regions; ir++)
      {
	 gr_solid = ProblemDataGrSolid(problem_data, region_indices[ir]);

	 ForSubgridI(sg, subgrids)
	 {
	    subgrid = SubgridArraySubgrid(subgrids,     sg);
	    pt_sub  = VectorSubvector(phase_thermalconductivity, sg);

	    ix = SubgridIX(subgrid);
	    iy = SubgridIY(subgrid);
	    iz = SubgridIZ(subgrid);

	    nx = SubgridNX(subgrid);
	    ny = SubgridNY(subgrid);
	    nz = SubgridNZ(subgrid);

	    r  = SubgridRX(subgrid);

	    ptdat = SubvectorData(pt_sub);

	    if ( fcn == CALCFCN )
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ips = SubvectorEltIndex(pt_sub, i, j, k);
		  ptdat[ips] = values[ir];
	       });
	    }
	    else /* fcn = CALCDER */
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ips = SubvectorEltIndex(pt_sub, i, j, k);
		  ptdat[ips] = 0.0;
	       });
	    }   /* End else clause */
	 }      /* End subgrid loop */
      }         /* End reion loop */
      break;
   }         /* End case 0 */

   case 1:  /* Emperical function #1*/
   {
      int     data_from_file;
      double *cdrys, *cwets;
      double  head, cdry, cwet;

      Vector *cwet_values, *cdry_values;

      dummy1 = (Type1 *)(public_xtra -> data);

      num_regions    = (dummy1 -> num_regions);
      region_indices = (dummy1 -> region_indices);
      cdrys          = (dummy1 -> cdrys);
      cwets          = (dummy1 -> cwets);
      data_from_file = (dummy1 -> data_from_file);

      if (data_from_file == 0) /* Soil parameters given by region */
      {
      for (ir = 0; ir < num_regions; ir++)
      {
	 gr_solid = ProblemDataGrSolid(problem_data, region_indices[ir]);

	 ForSubgridI(sg, subgrids)
	 {
	    subgrid = SubgridArraySubgrid(subgrids,     sg);
	    pt_sub  = VectorSubvector(phase_thermalconductivity, sg);
	    pp_sub  = VectorSubvector(phase_pressure,   sg);
	    ps_sub  = VectorSubvector(phase_saturation,   sg);

	    ix = SubgridIX(subgrid);
	    iy = SubgridIY(subgrid);
	    iz = SubgridIZ(subgrid);

	    nx = SubgridNX(subgrid);
	    ny = SubgridNY(subgrid);
	    nz = SubgridNZ(subgrid);

	    r  = SubgridRX(subgrid);

	    ptdat = SubvectorData(pt_sub);
	    ppdat = SubvectorData(pp_sub);
	    psdat = SubvectorData(ps_sub);

	    if ( fcn == CALCFCN )
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ips = SubvectorEltIndex(ps_sub, i, j, k);
		  ipt = SubvectorEltIndex(pt_sub, i, j, k);

		  cdry = cdrys[ir];
		  cwet = cwets[ir];

		     ptdat[ipt] = cdry + psdat[ips] * (cwet - cdry);
                     //printf("      k %d ptdat %e cdry %e psdat %e cwet %e \n",k,ptdat[ipt],cdry,psdat[ips],cwet);
	       });
	    }    /* End if clause */
	    else /* fcn = CALCDER */
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ipt = SubvectorEltIndex(pt_sub, i, j, k);
		  ips = SubvectorEltIndex(ps_sub, i, j, k);

		  cdry = cdrys[ir];
		  cwet     = cwets[ir];

		     ptdat[ipt] = psdat[ips] *(cwet - cdry); /* Here psdat contains the derivative dS/dp */
                     //printf("Deriv k %d ptdat %e cdry %e psdat %e cwet %e \n",k,ptdat[ipt],cdry,psdat[ips],cwet);
	       });
	    }   /* End else clause */
	 }      /* End subgrid loop */
      }         /* End loop over regions */
      }         /* End if data not from file */
      else
      {  
	 gr_solid = ProblemDataGrDomain(problem_data);
	 cwet_values = dummy1->cwet_values;
	 cdry_values = dummy1->cdry_values;

	 ForSubgridI(sg, subgrids)
	 {
	    subgrid = SubgridArraySubgrid(subgrids,     sg);
	    pt_sub  = VectorSubvector(phase_thermalconductivity, sg);
	    pp_sub  = VectorSubvector(phase_pressure,   sg);
	    ps_sub  = VectorSubvector(phase_saturation,   sg);

	    cwet_values_sub = VectorSubvector(cwet_values, sg);
	    cdry_values_sub = VectorSubvector(cdry_values, sg);

	    ix = SubgridIX(subgrid);
	    iy = SubgridIY(subgrid);
	    iz = SubgridIZ(subgrid);

	    nx = SubgridNX(subgrid);
	    ny = SubgridNY(subgrid);
	    nz = SubgridNZ(subgrid);

	    r  = SubgridRX(subgrid);

	    ptdat = SubvectorData(pt_sub);
	    psdat = SubvectorData(ps_sub);
	    ppdat = SubvectorData(pp_sub);

	    cwet_values_dat = SubvectorData(cwet_values_sub);
	    cdry_values_dat = SubvectorData(cdry_values_sub);

	    if ( fcn == CALCFCN )
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ips = SubvectorEltIndex(pt_sub, i, j, k);
		  ipt = SubvectorEltIndex(pt_sub, i, j, k);
		  ipp = SubvectorEltIndex(pp_sub, i, j, k);

		  cwet_index = SubvectorEltIndex(cwet_values_sub, i, j, k);
		  cdry_index = SubvectorEltIndex(cdry_values_sub, i, j, k);

		  cdry = cdry_values_dat[cdry_index];
		  cwet = cwet_values_dat[cwet_index];

		     ptdat[ipt] = cdry + psdat[ips] * (cwet - cdry);
	       });
	    }    /* End if clause */
	    else /* fcn = CALCDER */
	    {
	       GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
	       {
		  ipt = SubvectorEltIndex(pt_sub, i, j, k);
		  ipp = SubvectorEltIndex(pp_sub, i, j, k);

		  cwet_index = SubvectorEltIndex(cwet_values_sub, i, j, k);
		  cdry_index = SubvectorEltIndex(cdry_values_sub, i, j, k);

		  cdry = cdry_values_dat[cdry_index];
		  cwet = cwet_values_dat[cwet_index];

		     ptdat[ipt] = psdat[ipp] * (cwet - cdry);
	       });
	    }   /* End else clause */
	 }      /* End subgrid loop */
      }         /* End if data_from_file */
      break;
   }         /* End case 1 */

   }         /* End switch */

}

/*--------------------------------------------------------------------------
 * ThermalConductivityInitInstanceXtra
 *--------------------------------------------------------------------------*/

PFModule  *ThermalConductivityInitInstanceXtra(grid)
Grid      *grid;
{
   PFModule      *this_module   = ThisPFModule;
   PublicXtra    *public_xtra   = PFModulePublicXtra(this_module);
   InstanceXtra  *instance_xtra;

   Type1         *dummy1;

   if ( PFModuleInstanceXtra(this_module) == NULL )
      instance_xtra = ctalloc(InstanceXtra, 1);
   else
      instance_xtra = PFModuleInstanceXtra(this_module);

   /*-----------------------------------------------------------------------
    * Initialize data associated with argument `grid'
    *-----------------------------------------------------------------------*/

   if ( grid != NULL)
   {
      /* free old data */
      if ( (instance_xtra -> grid) != NULL )
      {
	 if (public_xtra -> type == 1)
	 {
	    dummy1 = (Type1 *)(public_xtra -> data);
	    if ( (dummy1->data_from_file) == 1 )
	    {
	       FreeVector(dummy1 -> cwet_values);
	       FreeVector(dummy1 -> cdry_values);
	    }
	 }
      }

      /* set new data */
      (instance_xtra -> grid) = grid;

      /* Uses a spatially varying field */
      if (public_xtra -> type == 1)
      {
	 dummy1 = (Type1 *)(public_xtra -> data);
	 if ( (dummy1->data_from_file) == 1 )
	 {
	    dummy1 -> cwet_values = NewVector(grid, 1, 1);
	    dummy1 -> cdry_values = NewVector(grid, 1, 1);
	 }
      }
   }


   /*-----------------------------------------------------------------------
    * Initialize data associated with argument `temp_data'
    *-----------------------------------------------------------------------*/


      /* Uses a spatially varying field */
      if (public_xtra->type == 1)
      {
	 dummy1 = (Type1 *)(public_xtra -> data);
	 if ( (dummy1->data_from_file) == 1)
	 {
            ReadPFBinary((dummy1 ->cdry_file), 
			 (dummy1 ->cdry_values));
	    ReadPFBinary((dummy1 ->cwet_file), 
			 (dummy1 ->cwet_values));
	 }
	 
      }

   PFModuleInstanceXtra(this_module) = instance_xtra;

   return this_module;
}

/*-------------------------------------------------------------------------
 * ThermalConductivityFreeInstanceXtra
 *-------------------------------------------------------------------------*/

void  ThermalConductivityFreeInstanceXtra()
{
   PFModule      *this_module   = ThisPFModule;
   PublicXtra    *public_xtra   = PFModulePublicXtra(this_module);
   InstanceXtra  *instance_xtra = PFModuleInstanceXtra(this_module);

   Type1   *dummy1;

    if ( public_xtra -> type ==1)
        {
                     dummy1 = (Type1 *)(public_xtra -> data);
                                 FreeVector(dummy1 -> cwet_values);
                                 FreeVector(dummy1 -> cdry_values);
        }
 

   if (instance_xtra)
   {
      free(instance_xtra);
   }
}

/*--------------------------------------------------------------------------
 * ThermalConductivityNewPublicXtra
 *--------------------------------------------------------------------------*/

PFModule   *ThermalConductivityNewPublicXtra()
{
   PFModule      *this_module   = ThisPFModule;
   PublicXtra    *public_xtra;

   Type0         *dummy0;
   Type1         *dummy1;

   int            num_regions, ir, ic;

   char *switch_name;
   char *region;
   
   char key[IDB_MAX_KEY_LEN];

   NameArray type_na;

   type_na = NA_NewNameArray("Constant Function1"); 

   public_xtra = ctalloc(PublicXtra, 1);
   
   switch_name = GetString("Phase.ThermalConductivity.Type");
   public_xtra -> type = NA_NameToIndex(type_na, switch_name);


   switch_name = GetString("Phase.ThermalConductivity.GeomNames");
   public_xtra -> regions = NA_NewNameArray(switch_name);
   
   num_regions = NA_Sizeof(public_xtra -> regions);

   switch((public_xtra -> type))
   {
      case 0:
      {
	 dummy0 = ctalloc(Type0, 1);
	 
	 dummy0 -> num_regions = num_regions;
	 
	 dummy0 -> region_indices = ctalloc(int,    num_regions);
	 dummy0 -> values         = ctalloc(double, num_regions);
	 
	 for (ir = 0; ir < num_regions; ir++)
	 {
	    region = NA_IndexToName(public_xtra -> regions, ir);
	    
	    dummy0 -> region_indices[ir] = 
	       NA_NameToIndex(GlobalsGeomNames, region);
	    
	    sprintf(key, "Geom.%s.ThermalConductivity.Value", region);
	    dummy0 -> values[ir] = GetDouble(key);
	 }
	 
	 (public_xtra -> data) = (void *) dummy0;
	 
	 break;
      }
      
      case 1:
      {
	 
	 dummy1 = ctalloc(Type1, 1);

	 sprintf(key, "Phase.ThermalConductivity.Function1.File");
	 dummy1->data_from_file = GetIntDefault(key,0);

	 if ( (dummy1->data_from_file) == 0)
	 {
	    dummy1 -> num_regions = num_regions;
	 
	    (dummy1 -> region_indices) = ctalloc(int,    num_regions);
	    (dummy1 -> cdrys        ) = ctalloc(double, num_regions);
	    (dummy1 -> cwets        ) = ctalloc(double, num_regions);
	    for (ir = 0; ir < num_regions; ir++)
	    {
	       region = NA_IndexToName(public_xtra -> regions, ir);
	    
	       dummy1 -> region_indices[ir] = 
	          NA_NameToIndex(GlobalsGeomNames, region);
	    
	       sprintf(key, "Geom.%s.ThermalConductivity.KDry", region);
	       dummy1 -> cdrys[ir] = GetDouble(key);
	    
	       sprintf(key, "Geom.%s.ThermalConductivity.KWet", region);
	       dummy1 -> cwets[ir] = GetDouble(key);
	    }

	    dummy1->cdry_file = NULL;
	    dummy1->cwet_file = NULL;
	    dummy1->cdry_values = NULL;
	    dummy1->cwet_values = NULL;
	 }
	 else
	 {
	    sprintf(key, "Geom.%s.ThermalConductivity.KDry.Filename", "domain");
	    dummy1->cdry_file = GetString(key);
	    sprintf(key, "Geom.%s.ThermalConductivity.KWet.Filename", "domain");
	    dummy1->cwet_file = GetString(key);
	      
	    dummy1->num_regions = 0;
	    dummy1->region_indices = NULL;
	    dummy1->cdrys = NULL;
	    dummy1->cwets = NULL;
	 }

	 (public_xtra -> data) = (void *) dummy1;
	 
	 break;
      }

      default:
      {
	 InputError("Error: invalid type <%s> for key <%s>\n",
		    switch_name, key);
      }
      
   }     /* End switch */
   
   NA_FreeNameArray(type_na);

   PFModulePublicXtra(this_module) = public_xtra;
   return this_module;
}

/*--------------------------------------------------------------------------
 * ThermalConductivityFreePublicXtra
 *--------------------------------------------------------------------------*/

void  ThermalConductivityFreePublicXtra()
{
   PFModule    *this_module   = ThisPFModule;
   PublicXtra  *public_xtra   = PFModulePublicXtra(this_module);

   Type0       *dummy0;
   Type1       *dummy1;

   int   num_regions, ir;

   if (public_xtra )
   {

      NA_FreeNameArray(public_xtra -> regions);

     switch((public_xtra -> type))
     {
     case 0:
     {
        dummy0 = (Type0 *)(public_xtra -> data);

	tfree(dummy0 -> region_indices);
	tfree(dummy0 -> values);
	tfree(dummy0);

	break;
     }

     case 1:
     {
        dummy1 = (Type1 *)(public_xtra -> data);

	if (dummy1->data_from_file == 1)
	{
	   FreeTempVector(dummy1->cdry_values);
	   FreeTempVector(dummy1->cwet_values);
	}

	tfree(dummy1 -> region_indices);
	tfree(dummy1 -> cdrys);
	tfree(dummy1 -> cwets);
	tfree(dummy1);

	break;
     }

     }   /* End of case statement */

     tfree(public_xtra);
   }
}

/*--------------------------------------------------------------------------
 * ThermalConductivitySizeOfTempData
 *--------------------------------------------------------------------------*/

int  ThermalConductivitySizeOfTempData()
{
   PFModule      *this_module   = ThisPFModule;
   PublicXtra    *public_xtra   = PFModulePublicXtra(this_module);

   Type1         *dummy1;

   int  sz = 0;

   if (public_xtra->type ==1)
   {
      dummy1 = (Type1 *)(public_xtra -> data);
      if ((dummy1->data_from_file) == 1)
      {
         /* add local TempData size to `sz' */
	 sz += SizeOfVector(dummy1 -> cwet_values);
	 sz += SizeOfVector(dummy1 -> cdry_values);
      }
   }

   return sz;
}