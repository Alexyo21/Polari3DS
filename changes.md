05/06/2024
- changed based on two issues n3ds per title config clock and l2 cache,
- now works also on nand and if you do not have   /n3ds folder it will create itself only on n3ds
- brought everything to latest commit on luma repo

17/07/2024
- fixed bug in confing
- unified option to patch two firm in one in config. 
- Implemented per game plugin loader from dullpointer
- add option for cor2extreme patch for cpu scheduler, it should turn cpu from preemptive to cooperative if I'm not mistaken 
Updated to latest commit and new luma version(also fake version) 
- new version 1.1.1 and new ini version also (updating it will overwrite you old lumae.ini)
- updated ini parser to latest version.
- made rosalina config also usable without sd card
- fixed bug where per game new3ds clock could not load correctly. 
- removed from sight chainloader in rosalina since it cause prefetch abort (work on it is needed)
- cleaned up ini file model since was too messy
- made bottom screen disable setting disaper in old2ds model in extra config
