#include <stdio.h>
#include <stdlib.h>
#include "percolate.h"

/*
*  Function to write a percolation map in greyscale Portable Grey Map
*  (PGM) format. The largest "ncluster" clusters are identified and
*  shown as shades of grey against a black background, with the
*  largest cluster shown in white.
*
*  Note that this version expects the map array to have been
*  dynamically allocated, e.g. using the arralloc() routine:
*
*  int **map;
*  map = (int **) arralloc(sizeof(int), 2, L, L);
*  ...
*  percwritedynamic("map.pgm", map, L, 1);
*/

#define MAXNCLUSTER 9   // Must be able to identify by a single digit

void percwritedynamic(char *percfile, int map[L][L], int l, int ncluster)
{
	FILE *fp;

	int i, j, clusterid, icluster, maxcluster, prevcluster, colour, npix;
	int *clustersize;
	int foundcluster[MAXNCLUSTER];

	static int pixperline = 32; // PGM format limits to 70 characters per line
	
	if (ncluster > MAXNCLUSTER)
	{
		printf("percwrite: WARNING ncluster too large, resetting to %d\n",
			MAXNCLUSTER);

		ncluster = MAXNCLUSTER;
	}

	if (ncluster > 1)
	{
		printf("percwrite: visualising the largest %d clusters\n", ncluster);
	}
	else
	{
		printf("percwrite: only visualising the largest cluster\n");
	}

	/*
	* Allocate the local clustersize array
	*/

	if ((clustersize = (int *)malloc(l*l*sizeof(int))) == NULL)
	{
		printf("percwrite: allocation of clustersize failed\n");
		return;
		//exit(1);
	}
	
	/*
	* Count up the size of each cluster
	*/

	for (i = 0; i<l*l; i++)
	{
		clustersize[i] = 0;
	}

	for (i = 0; i < l; i++)
	{
		for (j = 0; j < l; j++)
		{
			clusterid = map[i][j];

			if (clusterid > 0)
			{
				clustersize[clusterid]++;
			}
		}
	}
	
	/*
	* Find the size of the "ncluster" largest clusters (by brute force!)
	*/

	prevcluster = l*l + 1; // Larger than the largest possible cluster

	for (icluster = 0; icluster < ncluster; icluster++)
	{
		maxcluster = 0;

		for (i = 0; i<l*l; i++)
		{
			if (clustersize[i] > maxcluster && clustersize[i] < prevcluster)
			{
				maxcluster = clustersize[i];
			}
		}

		foundcluster[icluster] = maxcluster;
		prevcluster = maxcluster;
	}
	
	if (ncluster > 1)
	{
		printf("percwrite: cluster sizes are ");
	}
	else
	{
		printf("percwrite: maximum cluster size is ");
	}

	for (icluster = 0; icluster < ncluster - 1; icluster++)
	{
		printf("%d, ", foundcluster[icluster]);
	}
	printf("%d\n", foundcluster[ncluster - 1]);

	/*
	*  Write the file
	*/

	printf("percwrite: opening file <%s>\n", percfile);
	
	fp = fopen(percfile, "w");

	printf("percwrite: writing data ...\n");

	/*
	*  Start with the PGM header
	*/

	fprintf(fp, "P2\n");
	fprintf(fp, "%d %d\n%d\n", l, l, ncluster);

	/*
	*  Now write the cells to file so that map[0][0] is in the
	*  bottom-left-hand corner and map[l-1][l-1] is in the
	*  top-right-hand corner
	*/

	npix = 0;

	for (j = l - 1; j >= 0; j--)
	{
		for (i = 0; i<l; i++)
		{
			clusterid = map[i][j];

			/*
			* Write out the largest cluster(s), shading appropriately
			*/

			colour = 0;

			if (clusterid > 0)
			{
				for (icluster = 0; icluster < ncluster; icluster++)
				{
					if (clustersize[clusterid] == foundcluster[icluster])
					{
						// Largest (first) cluster is white

						colour = ncluster - icluster;
					}
				}
			}

			npix++;

			// Make sure lines wrap after "npix" pixels

			if (npix == 1)
			{
				fprintf(fp, "%1d", colour);
			}
			else if (npix < pixperline)
			{
				fprintf(fp, " %1d", colour);
			}
			else
			{
				fprintf(fp, " %1d\n", colour);
				npix = 0;
			}
		}
	}

	if (npix != 0) fprintf(fp, "\n");

	printf("percwrite: ... done\n");

	fclose(fp);
	printf("percwrite: file closed\n");

	/*
	* De-allocate the local clustersize array
	*/

	free(clustersize);
}
