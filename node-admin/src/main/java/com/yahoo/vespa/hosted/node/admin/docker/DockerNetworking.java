// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.node.admin.docker;// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

import com.yahoo.config.provision.NodeType;

/**
 * The types of network setup for the Docker containers.
 *
 * @author hakon
 */
public enum DockerNetworking {
    /** Each container has an associated macvlan bridge. */
    MACVLAN("vespa-macvlan"),

    /** Network Prefix-Translated networking. */
    NPT("vespa-bridge"),

    /** A host running a single container in the host network namespace. */
    HOST_NETWORK("host");

    private final String dockerNetworkMode;
    DockerNetworking(String dockerNetworkMode) {
        this.dockerNetworkMode = dockerNetworkMode;
    }

    public String getDockerNetworkMode() {
        return dockerNetworkMode;
    }

    public static DockerNetworking from(String cloud, NodeType nodeType, boolean hostAdmin) {
        if (cloud.equalsIgnoreCase("aws")) {
            return DockerNetworking.NPT;
        } else if (nodeType == NodeType.confighost || nodeType == NodeType.proxyhost) {
            return DockerNetworking.HOST_NETWORK;
        } else if (hostAdmin) {
            return DockerNetworking.NPT;
        } else {
            return DockerNetworking.MACVLAN;
        }
    }
}
