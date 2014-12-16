################################################################################
#                                                                              #
# EUDAT local specific functions                                               #
#                                                                              #
################################################################################

getEpicApiParameters(*credStoreType, *credStorePath, *epicApi, *serverID, *epicDebug) {
    *credStoreType="os";
    *credStorePath="/srv/irods/current/modules/B2SAFE/cmd/credentials_test";
    *epicApi="http://hdl.handle.net/";
    *serverID="irods://<hostnameWithFullDomain>:1247"; 
    *epicDebug=2; 

    EUDATAuthZ("$userNameClient#$rodsZoneClient", "read", *credStorePath, *response);
}

getAuthZParameters(*authZMapPath) {
    *authZMapPath="/srv/irods/current/modules/B2SAFE/cmd/authz.map.json";
}

getLogParameters(*logConfPath) {
    *logConfPath="/srv/irods/current/modules/B2SAFE/cmd/log.manager.conf";
}

#
# This function is used to set up some parameters for the site.
#
# Arguments:
# *protectArchive [OUT] Boolean, if 'true', the replicated file will become read only for the service user
# *archiveOwner [OUT] This is the iRods user owning the archive
#
# Author: S Coutin (CINES)
#
rp_getRpIngestParameters(*protectArchive, *archiveOwner) {
   *protectArchive = false;
   *archiveOwner = "rodsA";
}
