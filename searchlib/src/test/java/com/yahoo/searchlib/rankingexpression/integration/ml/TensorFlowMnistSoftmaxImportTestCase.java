// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.searchlib.rankingexpression.integration.ml;

import com.yahoo.searchlib.rankingexpression.ExpressionFunction;
import com.yahoo.searchlib.rankingexpression.RankingExpression;
import com.yahoo.tensor.Tensor;
import com.yahoo.tensor.TensorType;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * @author bratseth
 */
public class TensorFlowMnistSoftmaxImportTestCase {

    @Test
    public void testMnistSoftmaxImport() {
        TestableTensorFlowModel model = new TestableTensorFlowModel("test", "src/test/files/integration/tensorflow/mnist_softmax/saved");

        // Check constants
        assertEquals(2, model.get().largeConstants().size());

        Tensor constant0 = model.get().largeConstants().get("test_Variable_read");
        assertNotNull(constant0);
        assertEquals(new TensorType.Builder().indexed("d2", 784).indexed("d1", 10).build(),
                     constant0.type());
        assertEquals(7840, constant0.size());

        Tensor constant1 = model.get().largeConstants().get("test_Variable_1_read");
        assertNotNull(constant1);
        assertEquals(new TensorType.Builder().indexed("d1", 10).build(),
                     constant1.type());
        assertEquals(10, constant1.size());

        // Check (provided) functions
        assertEquals(0, model.get().functions().size());

        // Check required functions
        assertEquals(1, model.get().inputs().size());
        assertTrue(model.get().inputs().containsKey("Placeholder"));
        assertEquals(new TensorType.Builder().indexed("d0").indexed("d1", 784).build(),
                     model.get().inputs().get("Placeholder"));

        // Check signatures
        assertEquals(1, model.get().signatures().size());
        ImportedModel.Signature signature = model.get().signatures().get("serving_default");
        assertNotNull(signature);

        // ... signature inputs
        assertEquals(1, signature.inputs().size());
        TensorType argument0 = signature.inputArgument("x");
        assertNotNull(argument0);
        assertEquals(new TensorType.Builder().indexed("d0").indexed("d1", 784).build(), argument0);

        // ... signature outputs
        assertEquals(1, signature.outputs().size());
        ExpressionFunction output = signature.outputExpression("y");
        assertNotNull(output);
        assertEquals("add", output.getBody().getName());
        assertEquals("join(reduce(join(rename(Placeholder, (d0, d1), (d0, d2)), constant(test_Variable_read), f(a,b)(a * b)), sum, d2), constant(test_Variable_1_read), f(a,b)(a + b))",
                     output.getBody().getRoot().toString());
        assertEquals("{x=tensor(d0[],d1[784])}", output.argumentTypes().toString());

        // Test execution
        model.assertEqualResult("Placeholder", "MatMul");
        model.assertEqualResult("Placeholder", "add");
    }

}
