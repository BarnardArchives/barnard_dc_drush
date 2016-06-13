<?php

/**
 * @file
 * Custom drush commands for Barnard Digital Collections.
 *
 * Much of this was adapted from Robertson Library's drush code.
 * @see https://github.com/roblib/scripts/tree/master/drush/drupal7.
 */

require_once 'includes/barnard_dc.drush_ingest_helpers.inc';

/**
 * Implements hook_drush_command().
 */
function barnard_dc_drush_command() {
  return array(
    'barnard_dc_purge_object' => array(
      'description' => 'Purges an object from the repository.',
      'arguments' => array(
        'pid' => 'A unique identifier.',
      ),
      'aliases' => array('bcipo', 'bdcpo'),
    ),
    'barnard_dc_ingest_pubs' => array(
      'description' => 'Ingests student publications.',
      'arguments' => array(
        'md_file' => 'The metadata file.',
      ),
      'options' => array(
        'derivatives' => 'Whether to create derivatives.',
        'overwrite-existing' => 'Whether to overwrite existing objects.',
        'purge' => 'Whether to purge instead of ingesting.',
        'id' => 'Whether to import the publication corresponding to a certain internal identifier.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost bciip /path/to/pubs',
      ),
      'aliases' => array('bciip'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'barnard_dc_ingest_ms' => array(
      'description' => 'Ingests manuscripts.',
      'arguments' => array(
        'directory' => 'The directory in which the manuscript data/files are held.',
      ),
      'options' => array(
        'derivatives' => 'Whether to create derivatives.',
        'overwrite-existing' => 'Whether to overwrite existing objects.',
        'purge' => 'Whether to purge instead of ingesting.',
        'id' => 'Whether to import the publication corresponding to a certain internal identifier.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost bciim /path/to/mss',
      ),
      'aliases' => array('bciim'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'barnard_dc_generate_url_aliases' => array(
      'examples' => array(
        'drush -u 1 bcigua',
      ),
      'aliases' => array('bcigua'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'barnard_dc_check_ocr' => array(
      'examples' => array(
        'drush -u 1 bcico',
      ),
      'aliases' => array('bcico'),
    ),
    'barnard_dc_fix_pub_metadata' => array(
      'examples' => array(
        'drush bcifpm',
      ),
      'arguments' => array(
        'pid' => 'The pid of the object to process.',
      ),
      'aliases' => array('bcifpm'),
    ),
  );
}

/**
 * Purges one or more islandora objects from the fedora repository.
 *
 * @param string $pids
 *   The pid of the object to purge.
 */
function drush_barnard_dc_purge_object($object_id) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $connection = islandora_get_tuque_connection();
  $repository = $connection->repository;

  $pids = array($object_id);
  $object = islandora_object_load($object_id);

  if ($pages = islandora_paged_content_get_pages($object)) {
    $continue = drush_prompt(dt('Got paged object at pid = @pid. Purge pages?', array('@pid' => $object_id)));
    if (preg_match('/^y/', strtolower($continue)) === 1) {
      foreach ($pages as $page_id => $page) {
        $pids[] = $page_id;
      }
    }
  }

  foreach ($pids as $pid) {
    drush_log(dt('Purging object at pid = @pid...', array('@pid' => $pid)), 'ok');
    try {
      $repository->purgeObject($pid);
    }
    catch (Exception $e) {
      drush_log('Caught exception: @e', array('@e' => $e->getMessage()), 'warning');
    }
  }
}

/**
 * Ingests student publications.
 *
 * @param string $directory
 *   The directory containing publications to ingest.
 */
function drush_barnard_dc_ingest_pubs($directory) {
  $pub_id = drush_get_option('id');
  $purge = drush_get_option('purge');

  if (is_dir($directory)) {
    // Look at each file in the supplied (argument) directory.
    foreach (scandir($directory) as $file) {
      // The full path to this file.
      $path = $directory . '/' . $file;
      // The XML data file (should follow the format: [directory name].xml).
      $xml_file = $path . '/' . $file . '.xml';
      print "\$xml_file: $xml_file\n";
      // If we didn't get a good directory or XML file, move on.
      if (!is_dir($path) || preg_match('/^\./', $file) !== 0 || !file_exists($xml_file)) {
        continue;
      }
      $pub_data = _barnard_dc_pub_data($xml_file);
      $pid_title = str_replace(' ', '-', preg_replace('/[^a-z\d\s+]/', '', strtolower($pub_data['title'])));
      $pub_pid = 'bc:' . $pid_title;
      // Publication ids that constitute "exceptions" (different behavior
      // required).
      $exceptions = array('BC12-9_1959', 'BC12-26_1970', 'BC12-30_C19711972');
      $exception = in_array(array_pop(explode('/', $path)), $exceptions);
      // If pub object doesn't exist, create it.
      if (!islandora_object_load($pub_pid) && !$exception) {
        create_pub_object($pub_pid, $pub_data);
      }
      elseif ($exception) {
        $collection_pid = variable_get('barnard_dc_pubs_pid', 'islandora:1022');
        if (!$purge) {
          $book_pid = create_book_object($pub_pid, _barnard_dc_pub_data($xml_file, $collection_pid));
        }
        else {
          drush_barnard_dc_purge_object($pub_pid);
        }
      }
      $pub_obj = islandora_object_load($pub_pid);
      // If we got an id, only look at the corresponding dir.
      if (($pub_id && $file == $pub_id) || !$pub_id) {
        if ($exception) {
          // Yuk!
          if (!isset($book_pid)) {
            $book_pid = $pub_pid;
          }
          _barnard_dc_process_pub_pages($book_pid, $xml_file, $path, $purge);
        }
        // If non-exceptional, do the normal thing (TODO reverse this order?)
        else {
          print "Got pid: $pub_pid\n";
          // Create a book object corresponding to each pub.
          foreach (scandir($path) as $subfile) {
            $subpath = implode('/', array($path, $subfile));
            $xml_file = implode('/', array($subpath, $subfile)) . '.xml';
            if ((!is_dir($subpath) || preg_match('/^\./', $subdir) !== 0 || !file_exists($xml_file)) && !$exception) {
              continue;
            }
            $book_data = _barnard_dc_pub_data($xml_file, $pub_pid, 'book');
            $issue_pid = implode('-', array($pub_pid, $book_data['date']));
            print "Got pid: $issue_pid\n";
            if (!islandora_object_load($issue_pid)) {
              create_book_object($issue_pid, $book_data);
            }
            $issue_obj = islandora_object_load($issue_pid);
            // Create a page object for each page.
            _barnard_dc_process_pub_pages($issue_pid, $xml_file, $subpath, $purge);
          }
        }
      }
    }
  }
}

/**
 * Helper function: returns structured data about a publication.
 */
function _barnard_dc_pub_data($xml_file, $pid = '', $type = '', $page_number = '') {
  $xml = file_get_contents($xml_file);
  $mods = simplexml_load_string($xml);
  $data = array(
    'cm_pid' => 'islandora:collectionCModel',
    'xml' => $xml,
    'collection_pid' => variable_get('bc_islandora_pubs_pid', 'islandora:1022'),
    'date' => (string) $mods->originInfo->dateCreated,
    'title' => (string) $mods->titleInfo->title,
  );
  if ($type == 'page') {
    $data['genre'] = (string) $mods->genre;
    $data['title_host'] = (string) $mods->titleInfo->title;
    $data['host_id'] = (string) $mods->relatedItem[1]->identifier;
    $data['page_id'] = (string) $mods->identifier . '_' . str_pad($page_number, 3, '0', STR_PAD_LEFT);
  }
  return $data;
}

/**
 * Helper function: processes (ingests or purges) publication pages.
 */
function _barnard_dc_process_pub_pages($parent_pid, $parent_xml_file, $path, $purge = FALSE) {
  foreach (scandir($path) as $page) {
    if (preg_match('/^\./', $page) === 0 && preg_match('/\.tif$/', $page) !== 0) {
      $page_file = $path . '/' . $page;
      $page_number_padded = str_replace('.tif', '', end(explode('_', $page)));
      $page_number = preg_replace('/^0+/', '', $page_number_padded);
      $page_pid = $parent_pid . '-' . $page_number;
      $page_data = _barnard_dc_pub_data($parent_xml_file, $issue_pid, 'page', $page_number);
      $page_data['page_number'] = $page_number;
      $page_data['title'] .= ', page ' . $page_number;
      if (!islandora_object_load($page_pid) && !$purge) {
        print "Creating page: $page_pid\n";
        var_export($page_data);
        create_page_object($page_file, $page_data, $parent_pid, $page_pid, 'pub');
      }
      elseif ($purge) {
        drush_barnard_dc_purge_object($page_pid);
      }
    }
  }
}

/**
 * Ingests manuscripts.
 */
function drush_barnard_dc_ingest_ms($md_file) {
  if (file_exists($md_file)) {
    $md_fh = fopen($md_file, 'r');
    $id_option = drush_get_option('id');
    while (($data = fgetcsv($md_fh)) !== FALSE) {
      $ms_data = array();
      $ms_data['id'] = $data[0];
      // Skip this row if we got an id option and this id doesn't match.
      if ($id_option && $id_option != $ms_data['id']) {
        continue;
      }
      $ms_data_dir = dirname($md_file) . '/' . $ms_data['id'];
      if (is_dir($ms_data_dir)) {
        $continue = drush_prompt(dt("Entering directory: @dir. Continue?", array('@dir' => $ms_data_dir)));
        // Continue unless we got a response like "no" or "N"
        if (preg_match('/^n/', strtolower($continue)) === 1) {
          continue;
        }
        $ms_data['title'] = $data[1];
        $ms_mods_file = $ms_data_dir . '/' . $ms_data['id'] . '.xml';
        if (file_exists($ms_mods_file)) {
          $ms_data['xml'] = file_get_contents($ms_mods_file);
        }
        else {
          $ms_data['creator'] = $data[2];
          $ms_data['creator_uri'] = $data[3];
          $ms_data['resource_type'] = $data[4];
          $ms_data['genre'] = $data[5];
          $ms_data['genre_uri'] = $data[6];
          $ms_data['date'] = $data[7];
          // $ms_data['date_qualifier'] = $data[8];
          // NB start and end date only present if date is approx range...
          $ms_data['date_start'] = $data[8];
          $ms_data['date_end'] = $data[9];
          $ms_data['extent'] = $data[10];
          $ms_data['description'] = $data[11];
          $ms_data['location'] = $data[12];
          $ms_data['topic'] = $data[13];
          $ms_data['topic_uri'] = $data[14];
        }

        $ms_pid = _barnard_dc_ms_pid($ms_data['title']);

        if (!islandora_object_load($ms_pid)) {
          $ms_data['collection_pid'] = variable_get('barnard_dc_manuscripts_pid', 'islandora:manuscriptCollection');
          $ms_data['cm_pid'] = 'islandora:manuscriptCModel';
          $ms_data['xml'] = _barnard_dc_ms_mods($ms_data);
          // $ms_parent_pid = create_pub_object($ms_pid, $ms_data);
          $ms_parent_pid = create_pub_object('bcrw', $ms_data);
        }
        else {
          drush_log(dt('Object exists: @ms_pid', array('@ms_pid' => $ms_pid)), 'ok');
        }

        foreach (scandir($ms_data_dir) as $ms_file) {
          var_export($ms_file);
          if (preg_match('/\.tif$/', $ms_file)) {
            $page_padded = str_replace('.tif', '', array_pop(explode('_', $ms_file)));
            $page_number = preg_replace('/^0+/', '', $page_padded);
            $page_pid = $ms_pid . '-' . $page_number;
            $filepath = $ms_data_dir . '/' . $ms_file;
            $ms_data['id'] = $data[0] . '_' . $page_padded;
            $ms_data['page_number'] = $page_number;
            $ms_data['title'] = $data[1] . ', page ' . $page_number;
            $ms_data['cm_pid'] = $ms_pid;
            $ms_data['type'] = 'page';
            $ms_data['xml'] = _barnard_dc_ms_mods($ms_data);

            // TODO this is redundant now. Find another way to check if page exists?
            if (!islandora_object_load($page_pid)) {
              create_page_object($ms_data_dir . '/' . $ms_file, $ms_data, $ms_parent_pid, 'bcrw', 'ms');
            }
            else {
              drush_log(dt('Object exists: @page_pid', array('@page_pid' => $page_pid)), 'ok');
            }
          }
        }
        _barnard_dc_ms_transcript($ms_parent_pid);
      }
    }
  }
  else {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
    return;
  }
}

/**
 * Helper function: generate manuscript pid.
 */
function _barnard_dc_ms_pid($ms_title) {
  /*$ms_pid = 'bcrw:' . str_replace(' ', '-', preg_replace('/[^a-z\d\s+]/', '', strtolower($ms_title)));

  if (strlen($ms_pid) >= 60) {
    $ms_pid = drush_prompt(dt("Got a long title: @ms_title. Enter pid: ", array('@ms_title' => $ms_title)));
  }

  if (strlen($ms_pid) >= 60) {
    drush_log(dt("The pid you chose is too long! :("), 'error');
    $ms_pid = _barnard_dc_ms_pid($ms_title);
  }
  return $ms_pid;*/
  return 'bcrw';
}

/**
 * Helper function: generate manuscript MODS.
 */
function _barnard_dc_ms_mods($ms_data) {
  if (isset($ms_data['type']) && $ms_data['type'] == 'page') {
    $date_xml = '<originInfo>';
  }
  else {
    $date_xml = "<originInfo>
    <place>
      <placeTerm type=\"text\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/n79007751\">New York (N.Y.)</placeTerm>
    </place>
    <place>
      <placeTerm type=\"code\" authority=\"marccountry\">nyu</placeTerm>
    </place>
    <publisher>Online collection published by Barnard Archives and Special Collections, Barnard College.</publisher>";
    $extent_xml = "
  <physicalDescription>
    <form authority=\"marcform\">print</form>
    <form authority=\"marcform\">elecronic</form>
    <extent>{$ms_data['extent']}</extent>
    <digitalOrigin>reformatted digital</digitalOrigin>
    <internetMediaType>image/tiff</internetMediaType>
  </physicalDescription>";
    $description_xml = "
  <abstract>{$ms_data['description']}</abstract>";
    $location_xml = "
  <location displayLabel=\"not a PURL\">
    <physicalLocation authority=\"marcorg\">NNC</physicalLocation>
    <physicalLocation authority=\"marcorg\">NyNyBAW</physicalLocation>
    <physicalLocation>Barnard College Library (New York, N.Y.)</physicalLocation>
    <shelfLocator>{$ms_data['location']}</shelfLocator>
  </location>";
    $language_xml = "
  <language>
    <languageTerm type=\"text\">English</languageTerm>
    <languageTerm type=\"code\" authority=\"iso639-2b\">eng</languageTerm>
  </language>";
    $access_condition_xml = "
  <accessCondition type=\"useAndReproduction\">Contact the Archives at archives@barnard.edu for more information on access and use.</accessCondition>";
    $record_info_xml = "
  <recordInfo>
    <recordContentSource>Barnard Archives and Special Collections</recordContentSource>
    <recordContentSource authority=\"marcorg\">NNC</recordContentSource>
    <recordContentSource authority=\"marcorg\">NyNyBAW</recordContentSource>
    <languageOfCataloging>
      <languageTerm type=\"text\" authority=\"iso639-2b\">English</languageTerm>
      <languageTerm type=\"code\" authority=\"iso639-2b\">eng</languageTerm>
    </languageOfCataloging>
  </recordInfo>";
  }
  // TODO how to handle multiple creators?
  $creator_xml = "<name type=\"personal\" authority=\"\" authorityURI=\"\" valueURI=\"\">
    <namePart>{$ms_data['creator']}</namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">Creator</roleTerm>
    </role>
  </name>";
  $creator_uri_xml = isset($ms_data['creator_uri']) && !empty($ms_data['creator_uri']) ?
    "<name type=\"personal\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"{$ms_data['creator_uri']}\">
    <namePart></namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">{$ms_data['creator']}</roleTerm>
    </role>
  </name>" :
    "<name type=\"personal\" authority=\"local\">
    <namePart></namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">{$ms_data['creator']}</roleTerm>
    </role>
  </name>";

  // TODO this should be different depending whether a range is present.
  if (!empty($ms_data['date_start']) && !empty($ms_data['date_end'])) {
    $date_xml .= "
    <dateCreated encoding=\"w3cdtf\" keyDate=\"yes\" point=\"start\" qualifier=\"approximate\">{$ms_data['date_start']}</dateCreated>
    <dateCreated encoding=\"w3cdtf\" point=\"end\" qualifier=\"approximate\">{$ms_data['date_end']}</dateCreated>";
  }
  else {
    $date_xml .= "
    <dateCreated encoding=\"w3cdtf\" keyDate=\"yes\">{$ms_data['date']}</dateCreated>";  
  }
  $date_xml .= "
  </originInfo>";
  $mods_xml = "<mods xmlns=\"http://www.loc.gov/mods/v3\" xmlns:mods=\"http://www.loc.gov/mods/v3\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xsi:schemaLocation=\"http://www.loc.gov/mods/v3 http://www.loc.gov/mods/v3/mods-3-5.xsd\">
  <identifier type=\"local\">{$ms_data['id']}</identifier>
  <titleInfo>
    <title>{$ms_data['title']}</title>
  </titleInfo>
  {$creator_xml}
  {$creator_uri_xml}
  <typeOfResource>{$ms_data['resource_type']}</typeOfResource>
  <genre authority=\"aat\" authorityURI=\"http://vocab.getty.edu/aat\" valueURI=\"{$ms_data['genre_uri']}\">{$ms_data['genre']}</genre>
  {$date_xml}";
  if (isset($extent_xml)) {
    $mods_xml .= "$extent_xml";
  }
  if (isset($description_xml)) {
    $mods_xml .= "{$description_xml}";
  }
  if (isset($location_xml)) {
    $mods_xml .= "{$location_xml}";
  }
  $mods_xml .= "
  <subject authority=\"lcsh\" authorityURI=\"http://id.loc.gov/authorities/subjects\" valueURI=\"{$ms_data['topic_uri']}\">
    <topic>{$ms_data['topic']}</topic>
  </subject>";
  if (isset($language_xml)) {
    $mods_xml .= "{$language_xml}";
  }
  $mods_xml .= "
  <subject>
    <name type=\"corporate\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/n79065105\">
      <namePart>Barnard College</namePart>
    </name>
  </subject>
  <subject>
    <name type=\"corporate\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/no90023871\">
      <namePart>Barnard Center for Research on Women</namePart>
    </name>
  </subject>
  <subject authority=\"lcsh\" authorityURI=\"http://id.loc.gov/authorities/subjects\" valueURI=\"http://id.loc.gov/authorities/subjects/sh2008001560\">
    <topic>Second-wave feminism</topic>
  </subject>
  <relatedItem type=\"host\" displayLabel=\"Record Group\">
    <titleInfo>
      <title>Academic Departments and Programs</title>
    </titleInfo>
  </relatedItem>
  <relatedItem type=\"host\">
    <titleInfo>
      <title>Barnard Center for Research on Women</title>
    </titleInfo>
    <identifier type=\"local\">BC13-58</identifier>
    <typeOfResource>mixed material</typeOfResource>
  </relatedItem>";
  if (isset($access_condition_xml)) {
    $mods_xml .= "{$access_condition_xml}";
  }
  if (isset($record_info_xml)) {
    $mods_xml .= "{$record_info_xml}";
  }
  $mods_xml .= "\n</mods>";
  return $mods_xml;
}

function _barnard_dc_ms_transcript($ms_pid) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  if ($ms_obj = islandora_object_load($ms_pid)) {
    if (isset($ms_obj['TRANSCRIPT'])) {
      drush_log(dt('Transcript already exists for object: @ms_pid', array('@ms_pid' => $ms_pid)), 'ok');
      return;
    }
    else {
      $ms_ts = '';
    }
    foreach (islandora_paged_content_get_pages($ms_obj) as $page_pid => $page_data) {
      if ($page_obj = islandora_object_load($page_pid)) {
        if ($page_ocr = islandora_datastream_load('OCR', $page_obj)) {
          $ms_ts .= $page_ocr->getContent(NULL);
        }
      }
      else {
        drush_log(dt("Couldn't get page: @page_pid", array('@page_pid' => $page_pid)), 'warning');
      }
    }    
    // Create transcript datastream.
    $transcript_ds = $ms_obj->constructDatastream('TRANSCRIPT');
    $transcript_ds->label = 'Transcript';
    $transcript_ds->mimetype = 'text/plain';
    $transcript_ds->setContentFromString($ms_ts);
    $ms_obj->ingestDatastream($transcript_ds);
  }
  else {
    drush_log(dt("Couldn't load object at pid = @pid", array('@pid' => $ms_pid)), 'error');
  }
}

/**
 * Helper function to set and process derivative-creation batches.
 *
 * @see includes/barnard_dc.drush_batch.inc
 */
function _barnard_dc_create_derivatives($objects) {
  require_once 'includes/barnard_dc.drush_batch';
  foreach ($objects as $pid) {
    $book = islandora_object_load($pid);
    $pages = array_keys(islandora_paged_content_get_pages($book));
    // $img_batch = islandora_paged_content_create_images_batch($book, $pages);
    $img_batch = barnard_dc_create_images_batch($book, $pages);
    // $pdf_batch = islandora_paged_content_create_pdf_batch($book, $pages);
    $pdf_batch = barnard_dc_create_pdf_batch($book, $pages);
    $ocr_batch = islandora_paged_content_create_ocr_batch($book, $pages, array('language' => 'eng', 'preprocess' => FALSE));
    batch_set($img_batch);
    batch_set($pdf_batch);
    batch_set($ocr_batch);
  }
  drush_backend_batch_process();
}

/**
 * Generates URL aliases for objects in the repo that don't have them.
 *
 * (Requires islandora_pathauto module.)
 */
function drush_barnard_dc_generate_url_aliases() {
  $everything_query = <<<SPARQL
    SELECT DISTINCT ?object
    FROM <#ri>
    WHERE {
      ?object <fedora-model:hasModel> ?model
    }
SPARQL;
  $tuque = islandora_get_tuque_connection();
  $results = $tuque->repository->ri->query($everything_query, 'sparql');
  foreach ($results as $result) {
    $pid = $result['object']['value'];
    if ($object = islandora_object_load($pid)) {
      // Make the alias (if it doesn't already exist).
      $default_path = "islandora/object/{$pid}";
      if (drupal_get_path_alias($default_path) == $default_path) {
        $rels = $object->relationships->get();
        foreach ($rels as $rdf) {
          if (preg_match('/isMemberOf/', $rdf['predicate']['value']) && !in_array($rdf['object']['value'], array('islandora:root', 'islandora:collectionCModel'))) {
            drush_log(dt('Creating alias for @pid', array('@pid' => $object->id)), 'ok');
            islandora_pathauto_create_alias($object, 'bulkupdate');
          }
        }
      }
      else {
        drush_log(dt('Alias already exists at pid = @pid', array('@pid' => $object->id)), 'warning');
      }
    }
    else {
      drush_log(dt('Couldn\'t load object at pid = @pid', array('@pid' => $pid)), 'warning');
    }
  }
}

/**
 * Checks if book page objects have (H)OCR datastreams.
 */
function drush_barnard_dc_check_ocr($collection_pid = 'islandora:bookCollection') {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  // SPARQL query for isMemberOf book collection and hasModel bookCModel.
  $query = <<<EOQ
PREFIX islandora-rels-ext: <http://islandora.ca/ontology/relsext#>
PREFIX fedora-rels-ext: <info:fedora/fedora-system:def/relations-external#>
SELECT ?object
FROM <#ri>
WHERE {
  ?object fedora-rels-ext:isMemberOf <info:fedora/{$collection_pid}> ;
       <fedora-model:hasModel> <info:fedora/islandora:bookCModel> ;
}
EOQ;
  $tuque = islandora_get_tuque_connection();
  $results = $tuque->repository->ri->query($query, 'sparql');
  foreach ($results as $result) {
    $pid = isset($result['object']['value']) && $result['object']['type'] == 'pid' ?
      $result['object']['value'] : NULL;
    if ($pid && $book = islandora_object_load($pid)) {
      $pages = islandora_paged_content_get_pages($book);
      foreach ($pages as $p) {
        if ($page = islandora_object_load($p['pid'])) {
          if (!isset($page['OCR'])) {
            print "Didn't get OCR at book = {$book->id}\n";
            break;
          }
          elseif (!isset($page['HOCR'])) {
            print "Didn't get HOCR at book = {$book->id}\n";
            break;
          }
        }
      }
    }
  }
}

function _ms_default_mods($ms_obj) {
  $ms_title = $ms_obj->label;
  $ms_id = 'BC13-58_' . preg_replace('/[^\p{L}\p{N}]/u', '', str_replace("'", '', $ms_title));

  return <<<MODS
<?xml version="1.0" encoding="UTF-8"?>
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xlink="http://www.w3.org/1999/xlink" xsi:schemaLocation="http://www.loc.gov/mods/v3     http://www.loc.gov/mods/v3/mods-3-5.xsd">
  <titleInfo>
    <title>{$ms_title}</title>
  </titleInfo>
  <identifier type="local">{$ms_id}</identifier>
  <typeOfResource>text</typeOfResource>
  <genre authority="aat" authorityURI="http://vocab.getty.edu/aat" valueURI="http://vocab.getty.edu/aat/300248280">brochures</genre>
  <originInfo>
    <place>
      <placeTerm type="text" authorityURI="http://id.loc.gov/authorities/names" valueURI="http://id.loc.gov/authorities/names/n79007751">New York (N.Y.)</placeTerm>
    </place>
    <place>
      <placeTerm type="code" authority="marccountry">nyu</placeTerm>
    </place>
    <publisher>Online collection published by Barnard Archives and Special Collections, Barnard College.</publisher>
    <dateCreated encoding="w3cdtf" keyDate="yes" point="start" qualifier="approximate">1970</dateCreated>
    <dateCreated encoding="w3cdtf" point="end" qualifier="approximate">1980</dateCreated>
  </originInfo>
  <language>
    <languageTerm type="text" authority="iso639-2b">English</languageTerm>
    <languageTerm type="code" authority="iso639-2b">eng</languageTerm>
  </language>
  <physicalDescription>
    <form authority="marcform">print</form>
    <form authority="marcform">elecronic</form>
    <extent>2 pages</extent>
    <digitalOrigin>reformatted digital</digitalOrigin>
    <internetMediaType>image/tiff</internetMediaType>
  </physicalDescription>
  <abstract></abstract>
  <subject>
    <name type="corporate" authority="naf" authorityURI="http://id.loc.gov/authorities/names" valueURI="http://id.loc.gov/authorities/names/n79065105">
      <namePart>Barnard College</namePart>
    </name>
  </subject>
  <relatedItem type="host">
    <titleInfo>
      <title>Academic Departments and Programs Barnard Center for Research on Women</title>
    </titleInfo>
    <identifier type="local">BC37-02</identifier>
    <typeOfResource>still image</typeOfResource>
  </relatedItem>
  <accessCondition type="useAndReproduction">Contact the Archives at archives@barnard.edu for more information on access and use.</accessCondition>
  <location displayLabel="not a PURL">
    <physicalLocation authority="marcorg">NNC</physicalLocation>
    <physicalLocation authority="marcorg">NyNyBAW</physicalLocation>
    <physicalLocation>Barnard College Library (New York, N.Y.)</physicalLocation>
    <shelfLocator>test</shelfLocator>
  </location>
  <recordInfo>
    <recordContentSource>Barnard Archives and Special Collections</recordContentSource>
    <recordContentSource authority="marcorg">NNC</recordContentSource>
    <recordContentSource authority="marcorg">NyNyBAW</recordContentSource>
    <languageOfCataloging>
      <languageTerm type="text" authority="iso639-2b">English</languageTerm>
      <languageTerm type="code" authority="iso639-2b">eng</languageTerm>
    </languageOfCataloging>
  </recordInfo>
  <name type="corporate" authorityURI="http://id.loc.gov/authorities/names" valueURI="http://id.loc.gov/authorities/names/no90023871">
    <namePart>Barnard Center for Research on Women</namePart>
    <role>
      <roleTerm type="text" authority="marcrelator">Creator</roleTerm>
    </role>
  </name>
</mods>
MODS;
}

function drush_barnard_dc_fix_pub_metadata($pid) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $pub_obj = islandora_object_load($pid);
  $pub_mods_xml = isset($pub_obj['MODS']) ? simplexml_load_string($pub_obj['MODS']->getContent(NULL)) : '';
  $pub_obj_id = $pub_mods_xml->identifier;

  // Publication ids that constitute "exceptions" (different behavior
  // required).
  $exceptions = array('BC12-09_1959', 'BC12-26_1970', 'BC12-30_C19711972');
  $exception = in_array($pub_obj_id, $exceptions);
  $query_term = $exception ? 'islandora-rels-ext:isPageOf' : 'fedora-rels-ext:isMemberOf';

  $pub_pids = array();
  $tuque = islandora_get_tuque_connection();
  $repo = $tuque->repository;
  $query = <<<EOQ
PREFIX islandora-rels-ext: <http://islandora.ca/ontology/relsext#>
PREFIX fedora-rels-ext: <info:fedora/fedora-system:def/relations-external#>
SELECT ?object
FROM <#ri>
WHERE {
  ?object $query_term <info:fedora/{$pid}> ;
}
EOQ;

  $results = $repo->ri->query($query, 'sparql');
  foreach ($results as $r) {
    if (isset($r['object']) && $r['object']['type'] == 'pid') {
      $pub_pids[] = $r['object']['value'];
    }
  }

  // Whether to interrupt the loop.
  $interrupt = FALSE;

  foreach ($pub_pids as $pub_pid) {
    $issue_obj = islandora_object_load($pub_pid);
    $issue_rels = $issue_obj->relationships->get();

    foreach ($issue_rels as $r) {
      if ($r['predicate']['value'] == 'isMemberOf' && $parent_obj = islandora_object_load($r['object']['value'])) {
        foreach (islandora_paged_content_get_pages($issue_obj) as $pub_page) {
          try {
            drush_log(dt("Updating MODS at pid = @pid", array('@pid' => $pub_page['pid'])), 'ok');
            _update_page_mods($parent_obj, $pub_page['pid']);
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
      }
      elseif ($exception && !$interrupt) {
        $interrupt = TRUE;
        foreach ($pub_pids as $page_pid) {
          try {
            drush_log(dt("Updating MODS at pid = @pid", array('@pid' => $page_pid)), 'ok');
            _update_page_mods($pub_obj, $page_pid);
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
      }
      else {
        continue;
      }
    }
  }
}

function _update_page_mods($parent_obj, $page_pid) {
  $page_obj = islandora_object_load($page_pid);
  $page_mods = simplexml_load_string($page_obj['MODS']->getContent(NULL));
  // Get page number from title.
  preg_match('/[1-9]+$/', $page_mods->titleInfo->title, $page_match);
  unset($page_mods->relatedItem->title);
  $page_related_title = $page_mods->relatedItem[0]->titleInfo->title;
  $page_host_title = $page_mods->relatedItem[1]->titleInfo->title;
  if ($page_host_title != $parent_obj->label) {
    $page_mods->relatedItem[1]->titleInfo->title = $parent_obj->label;
  }
  if ($page_related_title != 'Student Publications') {
    $page_mods->relatedItem[0]->titleInfo->title = 'Student Publications';
  }
  if (!empty($page_match)) {
    $id_parts = explode('_', $page_mods->identifier);
    array_pop($id_parts);
    $id_parts[] = str_pad($page_match[0], 3, '0', STR_PAD_LEFT);
    $page_mods->identifier = implode($id_parts, '_') . "\n";
  }
  $page_obj['MODS']->content = $page_mods->asXML();
  $page_obj->ingestDatastream($page_obj['MODS']);
}
