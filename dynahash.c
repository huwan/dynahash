/*
** Dynamic hashing, after CACM April 1988 pp 446-457, by Per-Ake Larson.
** Coded into C, with minor code improvements, and with hsearch(3) interface,
** by ejp@ausmelb.oz, Jul 26, 1988: 13:16;
** also, hcreate/hdestroy routines added to simulate hsearch(3).
**
** These routines simulate hsearch(3) and family, with the important
** difference that the hash table is dynamic - can grow indefinitely
** beyond its original size (as supplied to hcreate()).
**
** Performance appears to be comparable to that of hsearch(3).
** The 'source-code' options referred to in hsearch(3)'s 'man' page
** are not implemented; otherwise functionality is identical.
**
** Compilation controls:
** DEBUG controls some informative traces, mainly for debugging.
** HASH_STATISTICS causes HashAccesses and HashCollisions to be maintained;
** when combined with DEBUG, these are displayed by hdestroy().
**
** Problems & fixes to ejp@ausmelb.oz. WARNING: relies on pre-processor
** concatenation property, in probably unnecessary code 'optimisation'.
*/

# include	<stdio.h>
/* # include	<search.h> */
# include	"search.h"
# include	<assert.h>
# include	<string.h>
# include	<stdlib.h>

/*
** Constants
*/

# define SegmentSize		256
# define SegmentSizeShift	8	/* log2(SegmentSize)	*/
# define DirectorySize		256
# define DirectorySizeShift	8	/* log2(DirectorySize)	*/
# define Prime1			37
# define Prime2			1048583
# define DefaultMaxLoadFactor	5

#define DEBUG 1
#define HASH_STATISTICS 1
/*
** Fast arithmetic, relying on powers of 2,
** and on pre-processor concatenation property
*/

# define MUL(x,y)		((x) << (y ## Shift))
# define DIV(x,y)		((x) >> (y ## Shift))
# define MOD(x,y)		((x) & ((y)-1))

/*
** local data templates
*/

typedef struct element
    {
    /*
    ** The user only sees the first two fields,
    ** as we pretend to pass back only a pointer to ENTRY.
    ** {S}he doesn't know what else is in here.
    */
    char		*Key;
    char		*Data;
    struct element	*Next;	/* secret from user	*/
    } Element,*Segment;

typedef struct
    {
    short	p;		/* Next bucket to be split	*/
    short	maxp;		/* upper bound on p during expansion	*/
    long	KeyCount;	/* current # keys	*/
    short	SegmentCount;	/* current # segments	*/
    short	MinLoadFactor;
    short	MaxLoadFactor;
    Segment	*Directory[DirectorySize];
    } HashTable;

typedef unsigned long	Address;

/*
** external routines
*/

/* extern char	*calloc(); */
/* extern int	free(); */

/*
** Entry points
*/

/* extern int	hcreate(); */
/* extern void	hdestroy(); */
/* extern ENTRY	*hsearch(); */

/*
** Internal routines
*/

static Address	Hash();
static void	ExpandTable();

/*
** Local data
*/

static HashTable	*Table = NULL;
# if HASH_STATISTICS
static long		HashAccesses, HashCollisions;
# endif

/*
** Code
*/

int
hcreate(Count)
unsigned Count;
{
    int		i;

    /*
    ** Adjust Count to be nearest higher power of 2,
    ** minimum SegmentSize, then convert into segments.
    */
    i = SegmentSize;
    while (i < Count)
	i <<= 1;
    Count = DIV(i,SegmentSize);

    Table = (HashTable*)calloc(sizeof(HashTable),1);
    if (Table == NULL)
	return(0);
    /*
    ** resets are redundant - done by calloc(3)
    **
    Table->SegmentCount = Table->p = Table->KeyCount = 0;
    */
    /*
    ** Allocate initial 'i' segments of buckets
    */
    for (i = 0; i < Count; i++)
    {
	Table->Directory[i] = (Segment*)calloc(sizeof(Segment),SegmentSize);
	if (Table->Directory[i] == NULL)
	{
	    hdestroy();
	    return(0);
	}
	Table->SegmentCount++;
    }
    Table->maxp = MUL(Count,SegmentSize);
    Table->MinLoadFactor = 1;
    Table->MaxLoadFactor = DefaultMaxLoadFactor;
# if DEBUG
    fprintf(
	    stderr,
	    "[hcreate] Table %x Count %d maxp %d SegmentCount %d\n",
	    Table,
	    Count,
	    Table->maxp,
	    Table->SegmentCount
	    );
# endif
# if HASH_STATISTICS
	HashAccesses = HashCollisions = 0;
# endif
    return(1);
}

void
hdestroy()
{
    int		i,j;
    Segment	*s;
    Element	*p,*q;

    if (Table != NULL)
    {
	for (i = 0; i < Table->SegmentCount; i++)
	{
	    /* test probably unnecessary	*/
	    if ((s = Table->Directory[i]) != NULL)
	    {
		for (j = 0; j < SegmentSize; j++)
		{
		    p = s[j];
		    while (p != NULL)
		    {
			q = p->Next;
			free((char*)p);
			p = q;
		    }
		}
		free(Table->Directory[i]); /* XXX */
	    }
	}
	free(Table);
	Table = NULL;
# if HASH_STATISTICS && DEBUG
	fprintf(
		stderr,
		"[hdestroy] Accesses %ld Collisions %ld\n",
		HashAccesses,
		HashCollisions
		);
# endif
    }
}

ENTRY *
hsearch(item,action)
ENTRY   item;
ACTION       action;	/* FIND/ENTER	*/
{
    Address	h;
    Segment	*CurrentSegment;
    int		SegmentIndex;
    int		SegmentDir;
    Segment	*p,q;

    assert(Table != NULL);	/* Kinder really than return(NULL);	*/
# if HASH_STATISTICS
    HashAccesses++;
# endif
    h = Hash(item.key);
    SegmentDir = DIV(h,SegmentSize);
    SegmentIndex = MOD(h,SegmentSize);
    /*
    ** valid segment ensured by Hash()
    */
    CurrentSegment = Table->Directory[SegmentDir];
    assert(CurrentSegment != NULL);	/* bad failure if tripped	*/
    p = &CurrentSegment[SegmentIndex];
    q = *p;
    /*
    ** Follow collision chain
    */
    while (q != NULL && strcmp(q->Key,item.key))
    {
	p = &q->Next;
	q = *p;
# if HASH_STATISTICS
	HashCollisions++;
# endif
    }
    if (
	q != NULL	/* found	*/
	||
	action == FIND	/* not found, search only	*/
	||
	(q = (Element*)calloc(sizeof(Element),1))
	==
	NULL		/* not found, no room	*/
	)
	return((ENTRY*)q);
    *p = q;			/* link into chain	*/
    /*
    ** Initialize new element
    */
    q->Key = item.key;
    q->Data = item.data;
    /*
    ** cleared by calloc(3)
    q->Next = NULL;
    */
    /*
    ** Table over-full?
    */
    if (++Table->KeyCount / MUL(Table->SegmentCount,SegmentSize) > Table->MaxLoadFactor)
	ExpandTable();	/* doesn't affect q	*/
    return((ENTRY*)q);
}

/*
** Internal routines
*/

static Address
Hash(Key)
char *Key;
{
    Address		h,address;
    register unsigned char	*k = (unsigned char*)Key;

    h = 0;
    /*
    ** Convert string to integer
    */
    while (*k)
	h = h*Prime1 ^ (*k++ - ' ');
    h %= Prime2;
    address = MOD(h,Table->maxp);
    if (address < Table->p)
	address = MOD(h,(Table->maxp << 1));	/* h % (2*Table->maxp)	*/
    return(address);
}

void
ExpandTable()
{
    Address	NewAddress;
    int		OldSegmentIndex,NewSegmentIndex;
    int		OldSegmentDir,NewSegmentDir;
    Segment	*OldSegment,*NewSegment;
    Element	*Current,**Previous,**LastOfNew;

    if (Table->maxp + Table->p < MUL(DirectorySize,SegmentSize))
    {
	/*
	** Locate the bucket to be split
	*/
	OldSegmentDir = DIV(Table->p,SegmentSize);
	OldSegment = Table->Directory[OldSegmentDir];
	OldSegmentIndex = MOD(Table->p,SegmentSize);
	/*
	** Expand address space; if necessary create a new segment
	*/
	NewAddress = Table->maxp + Table->p;
	NewSegmentDir = DIV(NewAddress,SegmentSize);
	NewSegmentIndex = MOD(NewAddress,SegmentSize);
	if (NewSegmentIndex == 0)
	    Table->Directory[NewSegmentDir] = (Segment*)calloc(sizeof(Segment),SegmentSize);
	NewSegment = Table->Directory[NewSegmentDir];
	/*
	** Adjust state variables
	*/
	Table->p++;
	if (Table->p == Table->maxp)
	{
	    Table->maxp <<= 1;	/* Table->maxp *= 2	*/
	    Table->p = 0;
	}
	Table->SegmentCount++;
	/*
	** Relocate records to the new bucket
	*/
	Previous = &OldSegment[OldSegmentIndex];
	Current = *Previous;
	LastOfNew = &NewSegment[NewSegmentIndex];
	*LastOfNew = NULL;
	while (Current != NULL)
	{
	    if (Hash(Current->Key) == NewAddress)
	    {
		/*
		** Attach it to the end of the new chain
		*/
		*LastOfNew = Current;
		/*
		** Remove it from old chain
		*/
		*Previous = Current->Next;
		LastOfNew = &Current->Next;
		Current = Current->Next;
		*LastOfNew = NULL;
	    }
	    else
	    {
		/*
		** leave it on the old chain
		*/
		Previous = &Current->Next;
		Current = Current->Next;
	    }
	}
    }
}

int main()
{

	ENTRY item, *found_item;
	char *token="hello world";
	item.key = token;
	item.data = NULL;

	hcreate(10);
    	found_item = hsearch(item, FIND);
    	if (found_item == NULL)
    	{
      		printf ("Not Found.\n");
    	}
    	found_item = hsearch(item, ENTER);
    	if (found_item == NULL)
    	{
      		printf ("Hash table error\n");
    	}
    	found_item = hsearch(item, FIND);
    	if (found_item != NULL)
    	{
      		printf ("Found\n");
    	}

	hdestroy();
	return 0;
}
