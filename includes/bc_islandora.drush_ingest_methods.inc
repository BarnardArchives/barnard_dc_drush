<?php

/**
 * @file
 * Ingest methods for `drush bc_islandora_ingest_objects`.
 *
 * @author Barnard Archives <[<archives@barnard.edu>]>
 *
 * @see https://archives.barnard.edu/finding-materials/collections-list
 */

require_once 'bc_islandora.drush_ingest_helpers.inc';

/**
 * Record Group BC15: Alumnae Scrapbooks, ingest method.
 *
 * Barnard maps Collection Record Group to Islandora Object Namespacing.
 *
 * @author Benjamin Rosner, gh br2490.
 *
 * @see https://archives.barnard.edu/finding-materials/collections-list#Record%20Group%2015:%20Alumnae%20Scrapbooks'
 * @see https://docs.google.com/spreadsheets/d/1VGg6RIV9httTIL5F-Vy_0EpWXfne4xTiG2mFZmO5dwA
 * @see _map_rg15_csv_rows()
 *
 * Models in use:
 * Diaries use book and page objects (book/page models).
 * Scrapbooks are compound objects of books and page models (compound model).
 *
 * Barnard College PID Structuring:
 *
 * * BC15:0, label: Alumnae Scrapbook Collection [collection]
 * * BC15-{ID}:0, label: {Alumna} Parent Collection. [collection]
 *   (relationship: is member of BC15:0)
 * * BC15-{ID}:[0-9]{n}, label: set by CSV initially and modified for pages,
 *   inclusions, etc. [objects] (relationships depend: is member of BC15-{ID},
 *   is part of, is sequence number, is member of escaped pid, etc...)
 *
 * Example ID patterns in our CSV:
 * BC15-12:0 describes a parent collection that needs to be created.
 * BC15-12_uniqueName - describes an object from record group BC15-12 and
 * has a unique name. Files are located {metadatadir}/BC15-12_uniqueName.
 * This is also the MODS->identifier.
 * BC15-15 - describes an object from record group BC15-15 and likely is a
 * solitary collection member. Files are located {metadatadir}/BC15-15.
 * This is also the MODS->identifier.
 *
 * A look at our anticipated folder structure:
 * ./ingest_metadata.csv (see Google Doc above, permission required.)
 * ./BC15-12_diaryxyz/scannedimg_ID_[0-9]{3}.tiff
 * ./BC15-12_diaryxyz/scannedimg_ID_[0-9]{3}.txt*
 * ./BC15-12_diaryxyz/scannedimg_ID_[0-9]{3}_INCLUSION_ID_[0-9]{3}.tiff
 * ./BC15-12_diaryxyz/scannedimg_ID_[0-9]{3}_INCLUSION_ID_[0-9]{3}.txt*
 *  where [0-9]{3} are padded page numbers, i.e.: 000, 001, 002...998, 999.
 *  *optional OCR stream to ingest
 */
function _bc_islandora_drush_ingest_rgBC15($md_file, $options) {
  // Return early if md_file is not found. This signals failure.
  if (!file_exists($md_file)) {
    drush_log(dt("File not found: @md_file", ['@md_file' => $md_file]), 'error');
    return;
  }

  // Read-in our CSV and map that data to an array.
  $csv = array_map('str_getcsv', file($md_file));
  // Drush options. Deprecated or under-appreciated, one or the other.
  $id_option = $options['identifier'];
  $ingest = $options['ingest'];
  // The default ingest collection for all scrapbooks.
  // @todo parametrize.
  $alumnae_scrapbook_collection = 'BC15:0';
  // The file extensions that should be treated as archival objects for ingest.
  $object_allowed_exts = ['tif', 'tiff'];
  // This collection has compound objects.
  module_load_include('inc', 'islandora_compound_object', 'includes/manage.form');
  $create_thumbs = variable_get('islandora_compound_object_thumbnail_child', TRUE);

  // CSV row processor.
  foreach ($csv as $row) {
    // "xpath" our csv to an associative array.
    $object_row = _map_rg15_csv_rows($row);

    $object_split = preg_split("(:|_)", $object_row['id']);

    // This alumna namespace (e.g., BC15-12, BC15-15, etc.).
    $object_namespace = $object_split[0];
    // This alumna parent collection (e.g., BC15-12:0, BC15-14:0, etc.).
    $alumna_collection_pid = $object_namespace . ':0';
    // A unique name for the obj (directory) or "0" if it is a collection.
    $object_unique_name = isset($object_split[1]) ? $object_split[1] : '';

    // Skip this row if we got an id option and this id doesn't match.
    if ($id_option && $id_option != $object_namespace) {
      drush_log("Skipped due to namespace mismatch: " . $csv_row['id'], 'error');
      continue;
    }

    // We use this more than once. Try to load the Alumna collection.
    $object_collection = islandora_object_load($alumna_collection_pid);

    // Alumna Collection creation and ingest.
    if ($object_unique_name === '0') {
      if (!isset($object_collection)) {
        drush_log(dt("Creating parent collection for @object at PID: @parent_collection.",
          [
            '@object' => $object_namespace,
            '@parent_collection' => $alumna_collection_pid,
          ]), 'ok');
        // Set required parameters for create_simple_object in our array.
        $object_row['content_model'] = 'islandora:collectionCModel';
        $object_row['id'] = $object_namespace;
        $object_row['xml'] = _bc_islandora_rg15_mods($object_row, 'collection');
        create_simple_object($alumna_collection_pid, $object_row, $alumnae_scrapbook_collection);
      }
      elseif (isset($object_collection)) {
        // This should be an option to overwrite, recreate, etc. but I'm lazy.
        drush_log(dt("The parent collection for @object already exists: @parent_collection.",
          [
            '@object' => $object_namespace,
            '@parent_collection' => $alumna_collection_pid,
          ]), 'ok');
      }
      continue;
    }

    // Normal object creation.
    if (!isset($object_collection)) {
      // We can get to this point if a collection hasn't yet been defined in the
      // CSV before objects are being ingested into it. Order matters.
      drush_log(dt("Missing parent collection: @parent_collection for: @object. A collection must be included in your ingest. Skipping @object...",
        [
          '@object' => $row['id'],
          '@parent_collection' => $alumna_collection_pid,
        ]), 'error');
      continue;
    }

    // Book and page object creation.
    $book_path = dirname($md_file) . '/' . $object_unique_name;

    if (!is_dir($book_path)) {
      drush_log(dt("Could not find @object to ingest at: @path", [
        '@path' => $book_path,
        '@object' => $object_unique_name,
      ]), 'error');
      return;
    }

    // Now we begin scouring directories and creating islandora objects.
    $current_scrapbook_pid = "";
    $is_compound = FALSE;
    $inclusion_pids = [];
    foreach (scandir($book_path) as $page_object) {
      $filepath = "$book_path/$page_object";
      $current_file = pathinfo($filepath);

      // Check if we have an allowed OBJECT, not something else (e.g., txt, xml)
      $is_object = in_array(strtolower($current_file['extension']), $object_allowed_exts);
      if (!$is_object) {
        continue;
      }

      // Copy CSV Row data, we need to modify some parts depending on the type
      // of object (e.g., changing title, pagenumb, etc.). self = this.
      $object_row_copy = $object_row;

      // File name string regex to vars and number of matches.
      $record_group = $scrapbook_name = $scrapbook_page = $inclusion_id = $inclusion_page = NULL;
      $matches = sscanf($current_file['filename'], '%[^_]_%[^_]_%d_%d_%d', $record_group, $scrapbook_name, $scrapbook_page, $inclusion_id, $inclusion_page);

      // Only valid cases based on file names should be 3 or 5 matches.
      // If we have 3 matches, ayy, forgetaboutit, but if we have 5 we have
      // inclusions, and well, we just cant forgetaboutit now. #nyc.
      if (!$matches == 3 || !$matches == 5) {
        // This signals complete failure. Appropriate error message. -br2490.
        throw new Exception("File naming convention error. oh jeez.", 1);
      }

      // Create these variables since we know we have an object and we'll wanna
      // do check for MODS, OCR, etc., files that go along with it.
      $mods_file = "$book_path/${current_file['filename']}.xml";
      // Unfortunately we end up testing if the mod file exists several times,
      // so rather than that, save it here:
      $mods_file_found = file_exists($mods_file);
      $ocr_file = "$book_path/${current_file['filename']}.txt";
      $page_padded = sprintf("%'.03d", $scrapbook_page);
      $inclusion_page_padded = isset($inclusion_page) ? sprintf("%'.03d", $inclusion_page) : NULL;

      // ## Book creation begins here.
      // The first page of each object signals make a book for them to chill in.
      if ($scrapbook_page == 1 || $inclusion_page == 1) {
        // Since we are on the first page of a book or an inclusion we need
        // to create a book object, and then ingest the its first page below.
        $object_row_copy['content_model'] = 'islandora:bookCModel';

        // Check for MODS file presence. This could be done better, looking at
        // it again with fresher eyes. -br2490.
        if ($mods_file_found) {
          drush_log("Found XML mods file for: " . $object_row_copy['id'], 'ok');
          $object_row_copy['xml'] = file_get_contents($xml_mods_file);
        }

        // Ingest of book objects. Hold PIDs of created books in variable.
        if ($matches == 3) {
          if (!$mods_file_found) {
            drush_log("Building RG15 book MODS for: ${object_row_copy['id']}", 'ok');
            $object_row_copy['xml'] = _bc_islandora_rg15_mods($object_row_copy, 'book');
          }
          $current_scrapbook_pid = create_simple_object($object_namespace, $object_row_copy, $alumna_collection_pid);
          drush_log("SUCCESS! scrapbook pid: $current_scrapbook_pid", 'ok');
        }
        else {
          if (!$mods_file_found) {
            drush_log("Building RG15 inclusion MODS for: ${object_row_copy['id']}", 'ok');
            // There are likely other variables that will need to be modified
            // before we call MODs generation, much less ingest.
            $object_row_copy['title'] = "${object_row['title']}, page $scrapbook_page, Inclusion $inclusion_id";
            $object_row_copy['xml'] = _bc_islandora_rg15_mods($object_row_copy, 'inclusion');
          }
          // This contains all of our inclusions.
          $is_compound = TRUE;
          $inclusion_pids["$scrapbook_page.$inclusion_id"] = $lazy = create_simple_object($object_namespace, $object_row_copy, $alumna_collection_pid);
          drush_log("SUCCESS! inclusion pid: $lazy", 'ok');
        }
      }
      // ## Book creation has ended.
      // ## Page creation begins here.
      // Setting object_row_copy variables that will be required by
      // create_complete_object().
      $object_row_copy['content_model'] = 'islandora:pageCModel';
      $object_row_copy['mime_label'] = 'TIFF Image';
      $object_row_copy['mime_type'] = 'image/tiff';
      $object_row_copy['ocr'] = file_exists($ocr_file) ? $ocr_file : '';
      // Set the destination of this page based on the number of matches.
      // Build out the MODs records for pages - we currently do NOT look
      // for .xml files at the page level. MODs at this level are scripted.
      if ($matches == 3) {
        $dest = $current_scrapbook_pid;
        $object_row_copy['id'] = "${object_row_copy['id']}_$page_padded";
        $object_row_copy['page_number'] = $scrapbook_page;
        $object_row_copy['title'] = "${object_row['title']}, page $scrapbook_page";
        drush_log("Building RG15 page MODS for: ${object_row_copy['id']}", 'ok');
        $object_row_copy['xml'] = _bc_islandora_rg15_mods($object_row_copy, 'page');
        drush_log("Scrapbook page: $page_padded -> destination: $dest", 'ok');
      }
      else {
        $dest = $inclusion_pids["$scrapbook_page.$inclusion_id"];
        $object_row_copy['id'] = "${object_row_copy['id']}_inclusion_$inclusion_id_$inclusion_page_padded";
        $object_row_copy['page_number'] = $inclusion_page;
        $object_row_copy['title'] = "${object_row['title']}, page $scrapbook_page, Inclusion $inclusion_id, page $inclusion_page";
        drush_log("Building RG15 inclusion MODS for: ${object_row_copy['id']}", 'ok');
        $object_row_copy['xml'] = _bc_islandora_rg15_mods($object_row_copy, 'inclusion_page');
        drush_log("Inclusion page: $inclusion_page_padded -> destination: $dest", 'ok');
      }
      // Now actually create the dang page!
      create_complete_object($object_namespace, $object_row_copy, $dest, $filepath);
    } // End scandir for objects.
    if ($is_compound) {
      drush_log(dt("We have a compound object - a book with inclusions. Building a Simple Compound Object (oxymoron) and associating scrapbook and inclusions."));

      // Grab a clean copy of our CSV row to create a compound obj. self = this.
      $object_row_copy = $object_row;

      $object_row_copy['content_model'] = ISLANDORA_COMPOUND_OBJECT_CMODEL;
      $object_row_copy['xml'] = _bc_islandora_rg15_mods($object_row_copy, 'book');
      $compound_object_pid = create_simple_object($object_namespace, $object_row_copy, $alumna_collection_pid);

      $parts = [];
      $parts[] = islandora_object_load($current_scrapbook_pid);
      foreach ($inclusion_pids as $inclusion_pid) {
        $parts[] = islandora_object_load($inclusion_pid);
      }
      islandora_compound_object_add_parent($parts, $compound_object_pid);
      if ($create_thumbs) {
        islandora_compound_object_update_parent_thumbnail($compound_object_pid);
      }
    } // End of Compound object handling.
  } // End of rows in CSV.
}

/**
 * Ingests manuscripts.
 */
function _bc_islandora_drush_ingest_manuscript($md_file, $options) {
  $id_option = $options['identifier'];
  if (file_exists($md_file)) {
    $md_fh = fopen($md_file, 'r');
    while (($data = fgetcsv($md_fh)) !== FALSE) {
      $ms_data = [];
      $ms_data['id'] = $data[0];
      // Skip this row if we got an id option and this id doesn't match.
      if ($id_option && $id_option != $ms_data['id']) {
        drush_log("Skipped due to ID mismatch: " . $ms_data['id'], 'error');
        continue;
      }
      $ms_data_dir = dirname($md_file) . '/' . $ms_data['id'];
      if (is_dir($ms_data_dir)) {
        $ms_data['title'] = $data[1];
        $ms_mods_file = $ms_data_dir . '/' . $ms_data['id'] . '.xml';
        // If we found an XML file that matches the naming convention, assume
        // it contains the MODS data we want.
        if (file_exists($ms_mods_file)) {
          drush_log("Found XML mods file for: " . $ms_data['id'], 'ok');
          $ms_data['xml'] = file_get_contents($ms_mods_file);
        }
        // Otherwise, generate MODS using the other csv fields.
        else {
          drush_log("Building mods from CSV MD file for: " . $ms_data['id'], 'ok');

          $ms_data['creator'] = $data[2];
          $ms_data['creator_uri'] = $data[3];
          $ms_data['creator1'] = $data[4];
          $ms_data['creator1_uri'] = $data[5];
          $ms_data['creator2'] = $data[6];
          $ms_data['creator2_uri'] = $data[7];
          $ms_data['creator3'] = $data[8];
          $ms_data['creator3_uri'] = $data[9];
          $ms_data['resource_type'] = $data[10];
          $ms_data['genre'] = $data[11];
          $ms_data['genre_uri'] = $data[12];
          $ms_data['date'] = $data[13];
          // $ms_data['date_qualifier'] = $data[14];
          // NB start and end date only present if date is approx range...
          $ms_data['date_start'] = $data[15];
          $ms_data['date_end'] = $data[16];
          $ms_data['extent'] = $data[17];
          $ms_data['description'] = $data[18];
          $ms_data['location'] = $data[19];
          $ms_data['topic'] = $data[20];
          $ms_data['topic_uri'] = $data[21];
        }

        $manuscript_ns = 'bcrw';

        // Create a new pub for all files using next available INT in namespace.
        if (!islandora_object_load($manuscript_ns)) {
          drush_log("Creating publication object for: " . $ms_data['id'], 'ok');
          $ms_data['collection_pid'] = variable_get('bc_islandora_manuscripts_pid', 'islandora:manuscriptCollection');
          $ms_data['cm_pid'] = 'islandora:manuscriptCModel';
          $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);
          $ms_parent_pid = create_pub_object($manuscript_ns, $ms_data);
        }
        else {
          drush_log(dt('Object exists: @manuscript_ns', ['@manuscript_ns' => $manuscript_ns]), 'ok');
        }

        foreach (scandir($ms_data_dir) as $ms_file) {
          // Create a page object for each tif file we find.
          if (preg_match('/\.tif$/', $ms_file)) {
            drush_log("Creating page object for: " . $ms_file, 'ok');
            $page_padded = str_replace('.tif', '', array_pop(explode('_', $ms_file)));
            $page_number = preg_replace('/^0+/', '', $page_padded);
            $page_pid = $manuscript_ns . '-' . $page_number;
            $filepath = $ms_data_dir . '/' . $ms_file;
            $ms_data['id'] = $data[0] . '_' . $page_padded;
            $ms_data['page_number'] = $page_number;
            $ms_data['title'] = $data[1] . ', page ' . $page_number;
            $ms_data['cm_pid'] = $manuscript_ns;
            $ms_data['type'] = 'page';
            $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);

            // Check if a text file is present as OCR.
            $ocr_file = $ms_data_dir . '/' . str_replace('.tif', '.txt', $ms_file);
            $ms_data['ocr'] = file_exists($ocr_file) ? $ocr_file : "";

            // TODO this is redundant now. Find another way to check if page
            // exists?
            if (!islandora_object_load($page_pid)) {
              create_page_object($ms_data_dir . '/' . $ms_file, $ms_data, $ms_parent_pid, $manuscript_ns, 'ms');
            }
            else {
              drush_log(dt('Object exists: @page_pid', ['@page_pid' => $page_pid]), 'ok');
            }
          }
        }
      }
    }
  }
  else {
    drush_log(dt("File not found: @md_file", ['@md_file' => $md_file]), 'error');
  }
}

/**
 * Ingests a newspaper object.
 *
 * Sample CSV data:
 * title,date,filename,"page number",type,extent
 * "Barnard Bulletin, October 1, 1990","October 1, 1990",,,issue,24
 */
function _bc_islandora_drush_ingest_newspaper($query_file, $options = []) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  // If the generate-csv option is specified or if the argument supplied is a
  // directory, generate a csv input file first.
  if ($options['generate-csv'] !== NULL || is_dir($query_file)) {
    $csv_file = _bc_islandora_drush_generate_newspaper_csv($query_file, $options);
    drush_log("Generated CSV file, please review it before continuing.", 'error');
    return;
  }
  else {
    $fh = fopen($query_file, 'r');
  }

  $path_name = empty($csv_file) ? dirname($query_file) : dirname($csv_file);
  $line_number = 0;
  $issues = [];

  if (!$fh) {
    drush_log("Couldn't open file.", 'error');
    return;
  }
  while (($data = fgetcsv($fh)) !== FALSE) {
    $line_number++;
    // Only parse the data if we aren't on the first line, which contains
    // column headings.
    if ($line_number > 1) {
      $type = $data[4];
      if ($type == 'issue') {
        // Sample issue pid: bc:bulletin-19901001.
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));

        // If the issue doesn't exist, create it.
        if (!islandora_object_load($issue_pid)) {
          $issue_pid = create_issue_object($issue_pid, $data);
        }
        // If it exists, give the user the option to overwrite it.
        // (N.B. this process will also allow the user to remove all
        // associated issue pages.)
        else {
          $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", ['@pid' => $issue_pid]));
          if (preg_match('/^y/', strtolower($continue)) === 1) {
            drush_bc_islandora_purge_object($issue_pid);
            $issue_pid = create_issue_object($issue_pid, $data);
          }
        }
        $issues[] = $issue_pid;
      }
      elseif ($type == 'page') {
        // Sample issue pid: bc:bulletin-19901001.
        // Sample page pid: bc:bulletin-19901001-1.
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));
        $page_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1])) . '-' . $data[3];

        if (!islandora_object_load($page_pid)) {
          create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
        }
        // If we found an existing page, prompt user on action.
        else {
          $continue = drush_prompt(dt("Page exists at pid = @pid. Overwrite? (y/n)", ['@pid' => $page_pid]));
          if (preg_match('/^y/', strtolower($continue)) === 1) {
            drush_bc_islandora_purge_object($page_pid);
            create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
          }
        }
      }
    }
  }
  // TODO should we keep this?
  // _bc_islandora_create_derivatives($issues);
}

/**
 * Ingests a book.
 */
function _bc_islandora_drush_ingest_book($query_file, $options = []) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $fh = fopen($query_file, 'r');
  $year = isset($options['year']) ? $options['year'] : NULL;
  $books = [];

  while (($data = fgetcsv($fh)) !== FALSE) {
    $csv_year = $data[3];
    if ($year && $year != $csv_year) {
      continue;
    }
    // Some tricky stuff follows to handle books with multiple "volumes".
    $vol_parts = explode(' ', $data[1]);
    array_shift($vol_parts);
    $book_pid = 'bc:yearbook-' . implode('.', $vol_parts);
    $pages = $data[12];

    if (!islandora_object_load($book_pid)) {
      drush_log("Creating book at pid = @pid", ['@pid' => $book_pid], 'ok');
      $book_pid = create_book_object($book_pid, $data);
    }
    else {
      $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", ['@pid' => $issue_pid]));
      if (preg_match('/^y/', strtolower($continue)) === 1) {
        drush_log(dt("Removing object: @pid", ['@pid' => $issue_pid]), 'ok');
        drush_bc_islandora_purge_object($issue_pid);
        $book_pid = create_book_object($book_pid, $data);
      }
      else {
        continue;
      }
    }
    $books[] = $book_pid;

    // Create each page.
    $file_path = dirname($query_file) . '/' . $csv_year;
    if (file_exists($file_path)) {
      $files = explode("\n", `ls $file_path/*.jp2`);
      foreach ($files as $fn) {
        if (!empty($fn)) {
          $page_number = preg_replace('/^0+/', '', preg_replace('/\.jp2$/', '', array_pop(explode('_', $fn))));
          $data[] = $page_number;
          $page_pid = $book_pid . '-' . $page_number;

          if (!islandora_object_load($page_pid)) {
            create_page_object($fn, $data, $book_pid, $page_pid, 'book');
          }
          else {
            drush_log(dt("Page exists at pid = @pid. Skipping...", ['@pid' => $page_pid]), 'warning');
          }
        }
      }
    }
    else {
      drush_log(dt("File not found: @file_path", ['@file_path' => $file_path]), 'error');
    }
  }
  // TODO should we keep this?
  _bc_islandora_create_derivatives($books);
}

/**
 * Ingests student publications.
 *
 * @param string $directory
 *   The directory containing publications to ingest.
 */
function _bc_islandora_drush_ingest_student_publication($directory, $options = []) {
  $pub_id = $options['identifier'];

  if (is_dir($directory)) {
    // Look at each file in the supplied (argument) directory.
    foreach (scandir($directory) as $file) {
      // The full path to this file.
      $path = $directory . '/' . $file;
      // The XML data file (should follow the format: [directory name].xml).
      $xml_file = $path . '/' . $file . '.xml';
      // If we didn't get a good directory or XML file, move on.
      if (!is_dir($path) || preg_match('/^\./', $file) !== 0 || !file_exists($xml_file)) {
        continue;
      }
      $pub_data = _bc_islandora_pub_data($xml_file);
      $pid_title = str_replace(' ', '-', preg_replace('/[^a-z\d\s+]/', '', strtolower($pub_data['title'])));
      $pub_pid = 'bc:' . $pid_title;
      // Publication ids that constitute "exceptions" (different behavior
      // required).
      $exceptions = ['BC12-9_1959', 'BC12-26_1970', 'BC12-30_C19711972'];
      $exception = in_array(array_pop(explode('/', $path)), $exceptions);
      // If pub object doesn't exist, create it.
      if (!islandora_object_load($pub_pid) && !$exception) {
        create_pub_object($pub_pid, $pub_data);
      }
      elseif ($exception) {
        $collection_pid = variable_get('bc_islandora_pubs_pid', 'islandora:1022');
        $book_pid = create_book_object($pub_pid, _bc_islandora_pub_data($xml_file, $collection_pid));
      }
      else {
        $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", ['@pid' => $pub_pid]));
        if (preg_match('/^y/', strtolower($continue)) === 1) {
          drush_log(dt("Removing object: @pub_pid", ['@pub_pid' => $pub_pid]), 'ok');
          drush_bc_islandora_purge_object($pub_pid);
          create_pub_object($pub_pid, $pub_data);
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
          _bc_islandora_process_pub_pages($book_pid, $xml_file, $path, $purge);
        }
        // If non-exceptional, do the normal thing (TODO reverse this order?)
        else {
          // Create a book object corresponding to each pub.
          foreach (scandir($path) as $subfile) {
            $subpath = implode('/', [$path, $subfile]);
            $xml_file = implode('/', [$subpath, $subfile]) . '.xml';
            if ((!is_dir($subpath) || preg_match('/^\./', $subdir) !== 0 || !file_exists($xml_file)) && !$exception) {
              continue;
            }
            $book_data = _bc_islandora_pub_data($xml_file, $pub_pid, 'book');
            $issue_pid = implode('-', [$pub_pid, $book_data['date']]);
            if (!islandora_object_load($issue_pid)) {
              create_book_object($issue_pid, $book_data);
            }
            else {
              $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", ['@pid' => $issue_pid]));
              if (preg_match('/^y/', strtolower($continue)) === 1) {
                drush_log(dt("Removing object: @pub_pid", ['@pub_pid' => $issue_pid]), 'ok');
                drush_bc_islandora_purge_object($issue_pid);
                create_book_object($issue_pid, $book_data);
              }
            }
            $issue_obj = islandora_object_load($issue_pid);
            // Create a page object for each page.
            _bc_islandora_process_pub_pages($issue_pid, $xml_file, $subpath);
          }
        }
      }
    }
  }
}
