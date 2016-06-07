# barnard_dc_drush

The module provides the following drush methods:

* bc_islandora_purge_object (bcipo)
* bc_islandora_ingest_newspapers (bciin)
* bc_islandora_ingest_books (bciib)
* bc_islandora_ingest_pubs (bciip)
* bc_islandora_generate_newspaper_csv (bcignc)
* bc_islandora_generate_url_aliases (bcigua)

The newspaper and book ingest commands were adapted from [Robertson Library's](https://github.com/roblib/scripts/tree/master/drush/drupal7) drush scripts. Our versions of these commands provide a few additional options, including one that specifies whether derivatives should be generated (`--derivatives`). To generate derivatives, we implement our own `bc_islandora_create_images_batch()` and `bc_islandora_create_pdf_batch()` functions (modified versions of `islandora_create_images_batch()` and `islandora_create_pdf_batch()`, respectively) in order to  give the batch-generated datastreams non-generic labels taken from MODS data (specifically, the "identifier" field).
