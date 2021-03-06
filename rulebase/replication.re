################################################################################
#                                                                              #
# Module Replication:                                                          #
#        - enable transfer single file                                         #   
#        - enable transfer collection                                          #
#        - enable transfer all files which have been logged                    #
#                 from the last transfers                                      #
#                                                                              #
################################################################################

# List of the functions:
#
# EUDATUpdateLogging(*status_transfer_success, *path_of_transfered_file, 
#               *target_transfered_file, *cause)
# EUDATCheckError(*path_of_transfered_file,*target_of_transfered_file)
# EUDATTransferSingleFile(*path_of_transfered_file,*target_of_transfered_file)
# EUDATTransferUsingFailLog(*buffer_length)
# EUDATCheckReplicas(*source, *destination)
#---- collection management ---
# EUDATTransferCollection(*path_of_transfered_coll,*target_of_transfered_coll,*incremental,*recursive)
# EUDATIntegrityCheck(*srcColl,*destColl)

#
# Update the logging files specific for EUDAT B2SAFE
# 
# Parameters:
#    *status_transfer_success    [IN] Status of transfered file (true or false)     
#    *path_of_transfered_file    [IN] path of transfered file in iRODS
#    *target_transfered_file     [IN] path of the destination file in iRODS
#    *cause                      [IN] cause of the failed transfer
#
# Author: Long Phan, JSC
# Modified by Claudio Cacciari, Cineca
#
EUDATUpdateLogging(*status_transfer_success, *path_of_transfered_file, 
              *target_transfered_file, *cause) {

    # Update Logging Statistical File
    *level = "INFO";
    *message = "*path_of_transfered_file::*target_transfered_file::" ++
               "*status_transfer_success::*cause";
    if (*status_transfer_success == bool("false")) {
        EUDATQueue("push", *message, 0);
        *level = "ERROR";
    } 
    EUDATLog(*message, *level);
}

#
# Checks differences about checksum and size between two files
# 
# Parameters:
#    *path_of_transfered_file    [IN] path of source file in iRODS
#    *target_of_transfered_file    [IN] path of target file in iRODS
#
# Author: Long Phan, JSC
# Modified by Claudio Cacciari, Cineca
#
EUDATCheckError(*path_of_transfered_file,*target_of_transfered_file) {
    
    *status_transfer_success = bool("true");
    *cause = "";
    *err = errorcode(msiObjStat(*target_of_transfered_file, *objStat));
    if (*err < 0) {
        *cause = "missing replicated object";
        *status_transfer_success = bool("false");
    } else if (EUDATCatchErrorChecksum(*path_of_transfered_file,*target_of_transfered_file) == bool("false")) {
        *cause = "different checksum";
        *status_transfer_success = bool("false");
    } else if (EUDATCatchErrorSize(*path_of_transfered_file,*target_of_transfered_file) == bool("false")) {
        *cause = "different size";
    *status_transfer_success = bool("false");
    } 
    if (*status_transfer_success == bool("false")) {
        logInfo("replication from *path_of_transfered_file to *target_of_transfered_file failed: *cause");
    } else {
        logInfo("replication from *path_of_transfered_file to *target_of_transfered_file succeeded");
    }

    EUDATUpdateLogging(*status_transfer_success,*path_of_transfered_file,
                       *target_of_transfered_file, *cause);
}

#
# Transfer single file
#
# Parameters:
#    *path_of_transfered_file    [IN] path of transfered file in iRODS
#    *target_of_transfered_file  [IN] destination of replication in iRODS
# 
# Author: Long Phan, JSC
# Modified by Claudio Cacciari, Cineca
#
EUDATTransferSingleFile(*path_of_transfered_file,*target_of_transfered_file) {
    
    logInfo("[EUDATTransferSingleFile] transfering *path_of_transfered_file to *target_of_transfered_file");    
        
    logDebug("query PID of DataObj *path_of_transfered_file");
    EUDATSearchPID(*path_of_transfered_file, *pid);

    if (*pid == "empty") {
        logDebug("PID is empty, no replication will be executed");        
        # Update Logging (Statistic File and Failed Files)
        *status_transfer_success = bool("false");
        EUDATUpdateLogging(*status_transfer_success,*path_of_transfered_file,
                      *target_of_transfered_file, "empty PID");                
    } else {
        logDebug("PID exist, Replication's beginning ...... ");
        getSharedCollection(*path_of_transfered_file,*sharedCollection);
        msiSplitPath(*path_of_transfered_file, *collection, *file);
        #msiReplaceSlash(*target_of_transfered_file, *controlfilename);
        EUDATReplaceSlash(*target_of_transfered_file, *controlfilename);
        logDebug("ReplicateFile: *sharedCollection*controlfilename");            
            
        # Catch Error CAT_NO_ACCESS_PERMISSION before replication
        EUDATCatchErrorDataOwner(*path_of_transfered_file,*status_identity);
            
        if (*status_identity == bool("true")) {
            *err = errorcode(triggerReplication("*sharedCollection*controlfilename.replicate",*pid,
                                                 *path_of_transfered_file,*target_of_transfered_file));
            if (*err < 0) {
                logDebug("triggerReplication failed with error code *err");
                *status_transfer_success = bool("false");
                EUDATUpdateLogging(*status_transfer_success,*path_of_transfered_file,
                              *target_of_transfered_file,"iRODS errorcode=*err");
            } else {
                logDebug("Perform the last checksum and checksize of transfered data object");        
                EUDATCheckError(*path_of_transfered_file,*target_of_transfered_file);
            }
        } else {
            logDebug("Action is canceled. Error is caught in function EUDATCatchErrorDataOwner"); 
            # Update fail_log                
            *status_transfer_success = bool("false");
            EUDATUpdateLogging(*status_transfer_success,*path_of_transfered_file,*target_of_transfered_file,
                               "no access permission");
        }            
    }        
}

#
# Transfer all data object saved in the logging system,
# according to the format: cause::path_of_transfer_file::target_of_transfer_file.
#
# Parameters:
#   *buffer_length    [IN] max number of failed transfers to process.
#                          It has to be > 1.
#
# Author: Long Phan, JSC
# Modified by Claudio Cacciari, Cineca;
#
EUDATTransferUsingFailLog(*buffer_length) {
    
    # get size of queue-log before transfer.    
    EUDATQueue("queuesize", *l, 0);
    EUDATQueue("pop", *messages, *buffer_length);
    
    *msg_list = split("*messages",",");    
    foreach (*message in *msg_list) {
        *message = triml(*message, "'");
        *message = trimr(*message, "'");
        *list = split("*message","::");

        *counter = 0;
        foreach (*item_LIST in *list) {
            if (*counter == 0) {*path_of_transfer_file = *item_LIST; }
            else if (*counter == 1) {*target_of_transfer_file = *item_LIST; }
            *counter = *counter + 1;
            if (*counter == 2) {break;}
        }
        EUDATTransferSingleFile(*path_of_transfer_file, *target_of_transfer_file);
    }

    # get size of queue-log after transfer. 
    EUDATQueue("queuesize", *la, 0);
    logInfo("AFTER TRANSFER: Length of Queue = *la");
    if (int(*l) == int(*la)) {
        logInfo("Transfer Data Objects got problems. No Data Objects have been transfered");
    } else {
        logInfo("There are *la Data Objects in Queue-log");
    }
      
}

#
# Check whether two files are available and identical
# If they are not identical, do the following:
#    1. find the pid of the object and modify checksum in the pid or create a new pid
#    2. create pid in the iCAT if it does not exist
#    3. add/update ROR in iCAT
#    4. trigger replication from source to destination
#
# Parameters:
#   *source         [IN]     source of the file
#   *destination    [IN]     destination of the file
# Author: Elena Erastova, RZG
#
EUDATCheckReplicas(*source, *destination) {
    logInfo("Check if 2 replicas have the same checksum. Source = *source, destination = *destination");
    if (EUDATCatchErrorChecksum(*source, *destination) == bool("false") || 
        EUDATCatchErrorSize(*source, *destination) == bool("false")) 
    {
        EUDATeiPIDeiChecksumMgmt(*source, *pid, bool("true"), bool("true"), 0);
        EUDATiRORupdate(*source, *pid);
        logInfo("replication from *source to *destination");
        getSharedCollection(*source,*collectionPath);
        #msiReplaceSlash(*destination,*filepathslash);
	EUDATReplaceSlash(*destination, *filepathslash);
        triggerReplication("*collectionPath*filepathslash.replicate",*pid,*source,*destination);
    }
}


####################################################################################
# Collection replication management                                                #
####################################################################################

#
# Parameters:
#     *path_of_transfered_coll     [IN] absolute iRODS path of the collection to be transfered
#     *target_of_transfered_coll   [IN] absolute iRODS path of the destination
#     *incremental                 [IN] bool('true') to perform an incremental transfer.
#     *recursive                   [IN] bool('true') to replicate the whole tree of subcollections.
#
# Author: Long Phan, JSC
#         Claudio Cacciari, Cineca
#
#
EUDATTransferCollection(*path_of_transfered_coll,*target_of_transfered_coll,*incremental,*recursive) {

    logInfo("[EUDATTransferCollection] Transfering *path_of_transfered_coll to *target_of_transfered_coll");

    #Verify that source input path is a collection
    msiGetObjType(*path_of_transfered_coll, *type);
    if (*type != '-c') {
        logError("Input path *path_of_transfered_coll is not a collection");
        fail;
    }
    #Verify that destination input path is a collection
    msiGetObjType(*target_of_transfered_coll, *type);
    if (*type != '-c') {
        logError("Input path *target_of_transfered_coll is not a collection");
        fail;
    }

    msiSplitPath(*path_of_transfered_coll,*sourceParent,*sourceChild);
    msiStrlen(*sourceParent,*pathLength);

    # if incremental
    if (*incremental == bool("true")) {
        # if recursive
        if (*recursive == bool("true")) {
            foreach(*Row in SELECT DATA_NAME,COLL_NAME WHERE COLL_NAME like '*path_of_transfered_coll/%') {
             	*objPath = *Row.COLL_NAME ++ '/' ++ *Row.DATA_NAME;
             	msiSubstr(*objPath, str(int(*pathLength)+1), "null", *subCollection);
             	*destination = "*target_of_transfered_coll/*subCollection";
                *err = errorcode(msiObjStat(*destination, *objStat));
                if (*err < 0) {
                    EUDATTransferSingleFile(*objPath, *destination);
                }
                else {
                    logDebug("Destination path *destination already exists, replication is not required");
                }
	    }
        }
        # if not recursive
        else {
            foreach(*Row in SELECT DATA_NAME,COLL_NAME WHERE COLL_NAME like '*path_of_transfered_coll/%') {
                *objPath = *Row.COLL_NAME ++ '/' ++ *Row.DATA_NAME;
                msiSubstr(*objPath, str(int(*pathLength)+1), "null", *subCollection);
                *depth_level = split(*subCollection, "/");
                *destination = "*target_of_transfered_coll/*subCollection";
                *err = errorcode(msiObjStat(*destination, *objStat));
                if (*err < 0 && size(*depth_level) == 2) {
                    EUDATTransferSingleFile(*objPath, *destination);
                }
                else {
                    logDebug("Destination path *destination already exists or");
                    logDebug(" *subCollection is beyond depth level 2: *depth_level");
                }
            }
        }
    }
    # if not incremental
    else {
        # if recursive
        if (*recursive == bool("true")) {
            foreach(*Row in SELECT DATA_NAME,COLL_NAME WHERE COLL_NAME like '*path_of_transfered_coll/%') {
                *objPath = *Row.COLL_NAME ++ '/' ++ *Row.DATA_NAME;
                msiSubstr(*objPath, str(int(*pathLength)+1), "null", *subCollection);
                *destination = "*target_of_transfered_coll/*subCollection";
                EUDATTransferSingleFile(*objPath, *destination);
            }
        }
        # if not recursive
        else {
	    foreach(*Row in SELECT DATA_NAME,COLL_NAME WHERE COLL_NAME like '*path_of_transfered_coll/%') {
                *objPath = *Row.COLL_NAME ++ '/' ++ *Row.DATA_NAME;
                msiSubstr(*objPath, str(int(*pathLength)+1), "null", *subCollection);
                *depth_level = split(*subCollection, "/");
                *destination = "*target_of_transfered_coll/*subCollection";
                if (size(*depth_level) == 2) {
                    EUDATTransferSingleFile(*objPath, *destination);
                }
                else {
                    logDebug("Sub collection path *subCollection is beyond depth level 2: *depth_level");
                }
            }
        }
    }
}

#
# This function will check all errors between source-Collection and destination-Collection
# Data with error will be pushed into fail.log which are able to be transfered later.
# Parameter:
# 	*srcColl	[IN]	Path of transfered Collection
#	*destColl	[IN]	Path of replicated Collection
#
# Author: Long Phan, JSC
#
EUDATIntegrityCheck(*srcColl,*destColl) {
        # Verify that input path is a collection
        msiGetObjType(*srcColl, *type);
        if (*type != '-c') {
            logError("Input path *srcColl is not a collection");
            fail;
        }
        msiGetObjType(*destColl, *type);
        if (*type != '-c') {
            logError("Input path *destColl is not a collection");
            fail;
	}
        msiSplitPath(*srcColl,*sourceParent,*sourceChild);
        msiStrlen(*srcColl,*pathLength);

        foreach(*Row in SELECT DATA_NAME,COLL_NAME WHERE COLL_NAME like '*path_of_transfered_coll/%') {
            *objPath = *Row.COLL_NAME ++ '/' ++ *Row.DATA_NAME;
            msiSubstr(*objPath, str(int(*pathLength)+1), "null", *subCollection);
            *destination = "*destColl/*subCollection";

                EUDATSearchPID(*objPath, *ppid);
                if (*ppid == "empty") {
                        logDebug("PPID is empty");
                        *status_transfer_success = bool("false");
                        *cause = "PPID is empty";
                        EUDATUpdateLogging(*status_transfer_success, *objPath, *destination, *cause);
                } else {
                        logDebug("PPID exists: *ppid");
                }
                # FIXME: is it possible to get CPID at source-location ?
                EUDATSearchPID(*destination, *cpid);
                if (*cpid == "empty") {
                        logDebug("CPID is empty");
                        *status_transfer_success = bool("false");
                        *cause = "CPID is empty";
                        EUDATUpdateLogging(*status_transfer_success, *objPath, *destination, *cause);
                } else {
                        logDebug("CPID exists: *cpid");
                }
                EUDATCheckError(*objPath, *destination);
            }
}
