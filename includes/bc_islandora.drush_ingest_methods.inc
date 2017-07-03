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
  // Return early if md_file is not found.
  if (!file_exists($md_file)) {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
    return;
  }

  // Drush options. Deprecated or under-appreciated, one or the other.
  $id_option = $options['identifier'];
  $csv = array_map('str_getcsv', file($md_file));

  // The default ingest collection for all scrapbooks.
  // Barnard maps Collection Record Group to Islandora Object Namespacing.
  $alumnae_scrapbook_collection = 'BC15:0';
  $obj_allowed_extensions = array('tif', 'tiff');

  foreach ($csv as $object_row) {
    $object_row = _map_rg15_csv_rows($object_row);

    $object_split = preg_split("(:|_)", $object_row['id']);

    // This alumna namespace (e.g., BC15-12, BC15-15, etc.).
    $object_namespace = $object_split[0];
    // This alumna parent collection (e.g., BC15-12:0, BC15-14:0, etc.).
    $object_collection_PID = $object_namespace . ':0';
    // A unique name for the obj (directory) or "0" if it is a collection.
    $object_unique_name = isset($object_split[1]) ? $object_split[1] : '';

    // Skip this row if we got an id option and this id doesn't match.
    if ($id_option && $id_option != $object_namespace) {
      drush_log("Skipped due to namespace mismatch: " . $csv_row['id'], 'error');
      continue;
    }

    // We use this more than once.
    $object_collection = islandora_object_load($object_collection_PID);

    // Alumna Collection creation and ingest.
    if ($object_unique_name === '0' && !isset($object_collection)) {
      drush_log(dt("Creating parent collection for @object at PID: @parent_collection.",
        array('@object' => $object_namespace, '@parent_collection' => $object_collection_PID)), 'ok');

      // Set required parameters for create_simple_object in our array.
      $object_row['content_model'] = 'islandora:collectionCModel';
      $object_row['id'] = $object_namespace;
      $object_row['xml'] = _bc_islandora_rg15_mods($object_row, 'collection');

      create_simple_object($object_collection_PID, $object_row, $alumnae_scrapbook_collection);
      continue;
    }
    // Provide this: elseif ('0' && TRUE) collection exists, overwrite?
    elseif (!isset($object_collection)) {
      // We can get to this point if a collection hasn't yet been defined in the
      // CSV before objects are being ingested into it. Order matters.
      drush_log(dt("Missing parent collection: @parent_collection for: @object. A collection must be included in your ingest. Skipping @object...",
        array('@object' => $row['id'], '@parent_collection' => $object_collection_PID)), 'error');
      // Here we're actually failing, but processing can continue as other
      // objects and collections could exist.
      continue;
    }

    // Book and page object creation.
    $book_path = dirname($md_file) . '/' . $object_unique_name;

    if (!is_dir($book_path)) {
      drush_log(dt("Could not find @object to ingest at: @path", array('@path' => $book_path, '@object' => $object_unique_name)), 'error');
      // This signals a total failure, return and halt execution.
      return;
    }

    $files = scandir($book_path);
    foreach ($files as $file) {
      $current_file = pathinfo("$book_path/$file");
      sscanf($current_file['filename'], '%[^_]_%[^_]_%d_%d_%d', $ns, $bookname, $page_number, $inclusion_id, $inclusion_page_number);

      $is_object = in_array(strtolower($current_file['extension']), $object_allowed_exts);
      if (!$is_object) {
        continue;
      }

      // Now the actual fun.
      if ((isset($bookname) && $page_number = 1) || (isset($inclusion_id) && $inclusion_page_number = 1)) {
        drush_log("Create a book $bookname because $page_number is 1. OR:
          \ncreate a book INCLUSION $inclusion_id because $inclusion_page_number is 1.");
        continue;
      }
      // then we handle pages just the same way, bookname stays the same,
      // while pagenumber is set.

    }

    // Halt execution here.
    return;

    // Book creation.
    // Create a new book object using the next available int.
    drush_log("Creating book object for: " . $object_row['id'], 'ok');
    $object_row['content_model'] = 'islandora:bookCModel';

    // Anticipated path to the possible MODS XML for this book.
    $xml_mods_file = $book_path . '/' . $object_row['id'] . '.xml';
    // If we find that file set it as XML, otherwise build mods.
    if (file_exists($xml_mods_file)) {
      drush_log("Found XML mods file for: " . $object_row['id'], 'ok');
      $object_row['xml'] = file_get_contents($xml_mods_file);
    }
    else {
      drush_log("Building MODS from CSV record for: " . $object_row['id'], 'ok');
      $object_row['xml'] = _bc_islandora_rg15_mods($csv_row, 'book');
    }
    // Using $object_namespace to grab next available INT in the namespace.
    $parent_object = create_simple_object($object_namespace, $object_row, $object_collection_PID);
    $is_compound = FALSE;
    // End Book Creation.


    // Walk this book's directory and start creating objects from each.
    foreach (scandir($book_path) as $page_object) {

      $current_file = pathinfo("$object_directory/$page_object");
      $is_object = in_array(strtolower($current_file['extension']), $object_allowed_exts);

      if (!$is_object) {
        continue;
      }

      $object_file_row = $object_row;
      drush_log(dt('Creating a page for: @filename', array('@filename' => $current_file['basename'])), 'notice');
      sscanf($current_file['filename'], '%[^_]_%[^_]_%d_%d_%d', $ns, $bookname, $page_number, $inclusion_id, $inclusion_page_number);
      $page_padded = sprintf("%'.03d", $page_number);

       if ($inclusion_id && $inclusion_page) {
        $is_compound = TRUE;
        // DO WORK!
      }

      // The path to our TIFF.
      $filepath = "$book_path/$page_object";
      $object_file_row['id'] = $object_row['id'] . '_' . $page_padded;
      $object_file_row['page_number'] = $page_number;
      $object_file_row['title'] = $object_row['title'] . ', page ' . $page_number;
      $object_file_row['content_model'] = 'islandora:pageCModel';
      $object_file_row['mime_label'] = 'TIFF Image';
      $object_file_row['mime_type'] = 'image/tiff';
      $object_file_row['xml'] = _bc_islandora_rg15_mods($object_row, 'page');

      // Check if a text file is present as OCR.
      $ocr_file = $book_path . '/' . str_replace('.tif', '.txt', $page_object);
      $object_file_row['ocr'] = file_exists($ocr_file) ? $ocr_file : "";

      // Use $object_namespace to use next available INT in the namespace,
      // else be more specific here to declare your own PIDs.
      create_complete_object($object_namespace, $object_file_row, $parent_object, $filepath);
    }
  }
}

/**
 * Ingests manuscripts.
 */
function _bc_islandora_drush_ingest_manuscript($md_file, $options) {
  $id_option = $options['identifier'];
  if (file_exists($md_file)) {
    $md_fh = fopen($md_file, 'r');
    while (($data = fgetcsv($md_fh)) !== FALSE) {
      $ms_data = array();
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
          drush_log(dt('Object exists: @manuscript_ns', array('@manuscript_ns' => $manuscript_ns)), 'ok');
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
              drush_log(dt('Object exists: @page_pid', array('@page_pid' => $page_pid)), 'ok');
            }
          }
        }
      }
    }
  }
  else {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
  }
}

/**
 * Ingests a newspaper object.
 *
 * Sample CSV data:
 * title,date,filename,"page number",type,extent
 * "Barnard Bulletin, October 1, 1990","October 1, 1990",,,issue,24
 */
function _bc_islandora_drush_ingest_newspaper($query_file, $options = array()) {
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
  $issues = array();

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
          $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
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
          $continue = drush_prompt(dt("Page exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $page_pid)));
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
function _bc_islandora_drush_ingest_book($query_file, $options = array()) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $fh = fopen($query_file, 'r');
  $year = isset($options['year']) ? $options['year'] : NULL;
  $books = array();

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
      drush_log("Creating book at pid = @pid", array('@pid' => $book_pid), 'ok');
      $book_pid = create_book_object($book_pid, $data);
    }
    else {
      $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
      if (preg_match('/^y/', strtolower($continue)) === 1) {
        drush_log(dt("Removing object: @pid", array('@pid' => $issue_pid)), 'ok');
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
            drush_log(dt("Page exists at pid = @pid. Skipping...", array('@pid' => $page_pid)), 'warning');
          }
        }
      }
    }
    else {
      drush_log(dt("File not found: @file_path", array('@file_path' => $file_path)), 'error');
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
function _bc_islandora_drush_ingest_student_publication($directory, $options = array()) {
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
      $exceptions = array('BC12-9_1959', 'BC12-26_1970', 'BC12-30_C19711972');
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
        $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $pub_pid)));
        if (preg_match('/^y/', strtolower($continue)) === 1) {
          drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $pub_pid)), 'ok');
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
            $subpath = implode('/', array($path, $subfile));
            $xml_file = implode('/', array($subpath, $subfile)) . '.xml';
            if ((!is_dir($subpath) || preg_match('/^\./', $subdir) !== 0 || !file_exists($xml_file)) && !$exception) {
              continue;
            }
            $book_data = _bc_islandora_pub_data($xml_file, $pub_pid, 'book');
            $issue_pid = implode('-', array($pub_pid, $book_data['date']));
            if (!islandora_object_load($issue_pid)) {
              create_book_object($issue_pid, $book_data);
            }
            else {
              $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
              if (preg_match('/^y/', strtolower($continue)) === 1) {
                drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $issue_pid)), 'ok');
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
