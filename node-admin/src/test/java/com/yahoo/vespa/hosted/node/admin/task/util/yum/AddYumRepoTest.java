// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

package com.yahoo.vespa.hosted.node.admin.task.util.yum;

import com.yahoo.vespa.hosted.node.admin.component.TaskContext;
import com.yahoo.vespa.hosted.node.admin.task.util.file.TestFileSystem;
import com.yahoo.vespa.hosted.node.admin.task.util.file.UnixPath;
import org.junit.Test;

import java.nio.file.FileSystem;
import java.time.Instant;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

public class AddYumRepoTest {
    @Test
    public void converge() throws Exception {
        String repositoryId = "repoid";
        String name = "name";
        String baseurl = "http://foo.com/bar";
        boolean enabled = true;

        AddYumRepo addYumRepo = new AddYumRepo(
                repositoryId,
                name,
                baseurl,
                enabled);

        TaskContext context = mock(TaskContext.class);

        FileSystem fileSystem = TestFileSystem.create();
        when(context.fileSystem()).thenReturn(fileSystem);

        assertTrue(addYumRepo.converge(context));

        UnixPath unixPath = new UnixPath(fileSystem.getPath("/etc/yum.repos.d/" + repositoryId + ".repo"));
        String content = unixPath.readUtf8File();
        assertEquals("# This file was generated by node admin\n" +
                "# Do NOT modify this file by hand\n" +
                "\n" +
                "[repoid]\n" +
                "name=name\n" +
                "baseurl=http://foo.com/bar\n" +
                "enabled=1\n" +
                "gpgcheck=0\n", content);
        Instant lastModifiedTime = unixPath.getLastModifiedTime();

        // Second time is a no-op
        assertFalse(addYumRepo.converge(context));
        assertEquals(lastModifiedTime, unixPath.getLastModifiedTime());
    }

}