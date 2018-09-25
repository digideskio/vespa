// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.searchlib.rankingexpression;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.yahoo.log.event.Collection;
import com.yahoo.searchlib.rankingexpression.rule.ExpressionNode;
import com.yahoo.searchlib.rankingexpression.rule.SerializationContext;
import com.yahoo.tensor.TensorType;
import com.yahoo.text.Utf8;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Collections;
import java.util.Deque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

/**
 * A function defined by a ranking expression, optionally containing type information
 * for inputs and outputs.
 *
 * Immutable, but note that ranking expressions are *not* immutable.
 *
 * @author Simon Thoresen Hult
 * @author bratseth
 */
public class ExpressionFunction {

    private final String name;
    private final ImmutableList<String> arguments;

    /** Types of the inputs, if known. The keys here is any subset (including empty and identity) of the argument list */
    private final ImmutableMap<String, TensorType> argumentTypes;
    private final RankingExpression body;

    private final Optional<TensorType> returnType;

    /**
     * Constructs a new function with no arguments
     *
     * @param name the name of this function
     * @param body the ranking expression that defines this function
     */
    public ExpressionFunction(String name, RankingExpression body) {
        this(name, Collections.emptyList(), body);
    }

    /**
     * Constructs a new function
     *
     * @param name the name of this function
     * @param arguments its argument names
     * @param body the ranking expression that defines this function
     */
    public ExpressionFunction(String name, List<String> arguments, RankingExpression body) {
        this(name, arguments, body, ImmutableMap.of(), Optional.empty());
    }

    public ExpressionFunction(String name, List<String> arguments, RankingExpression body,
                              Map<String, TensorType> argumentTypes, Optional<TensorType> returnType) {
        this.name = Objects.requireNonNull(name, "name cannot be null");
        this.arguments = arguments==null ? ImmutableList.of() : ImmutableList.copyOf(arguments);
        this.body = Objects.requireNonNull(body, "body cannot be null");
        if ( ! this.arguments.containsAll(argumentTypes.keySet()))
            throw new IllegalArgumentException("Argument type keys must be a subset of the argument keys");
        this.argumentTypes = ImmutableMap.copyOf(argumentTypes);
        this.returnType = Objects.requireNonNull(returnType, "returnType cannot be null");
    }

    public String getName() { return name; }

    /** Returns an immutable list of the arguments of this */
    public List<String> arguments() { return arguments; }

    public RankingExpression getBody() { return body; }

    /** Returns the types of the arguments of this, if specified. The keys of this may be any subset of the arguments */
    public Map<String, TensorType> argumentTypes() { return argumentTypes; }

    /** Returns the return type of this, or empty if not specified */
    public Optional<TensorType> returnType() { return returnType; }

    public ExpressionFunction withName(String name) {
        return new ExpressionFunction(name, arguments, body, argumentTypes, returnType);
    }

    /** Returns a copy of this with the body changed to the given value */
    public ExpressionFunction withBody(RankingExpression body) {
        return new ExpressionFunction(name, arguments, body, argumentTypes, returnType);
    }

    public ExpressionFunction withReturnType(TensorType returnType) {
        return new ExpressionFunction(name, arguments, body, argumentTypes, Optional.of(returnType));
    }

    public ExpressionFunction withArgumentTypes(Map<String, TensorType> argumentTypes) {
        return new ExpressionFunction(name, arguments, body, argumentTypes, returnType);
    }

    /**
     * Creates and returns an instance of this function based on the given
     * arguments. If function calls are nested, this call may produce
     * additional functions.
     *
     * @param context the context used to expand this
     * @param argumentValues the arguments to instantiate on.
     * @param path the expansion path leading to this.
     * @return the script function instance created.
     */
    public Instance expand(SerializationContext context, List<ExpressionNode> argumentValues, Deque<String> path) {
        Map<String, String> argumentBindings = new HashMap<>();
        for (int i = 0; i < arguments.size() && i < argumentValues.size(); ++i) {
            argumentBindings.put(arguments.get(i), argumentValues.get(i).toString(new StringBuilder(), context, path, null).toString());
        }
        return new Instance(toSymbol(argumentBindings), body.getRoot().toString(new StringBuilder(), context.withBindings(argumentBindings), path, null).toString());
    }

    /**
     * Returns a symbolic string that represents this function with a given
     * list of arguments. The arguments are mangled by hashing the string
     * representation of the argument expressions.
     *
     * @param  argumentBindings the bound arguments to include in the symbolic name.
     * @return the symbolic name for an instance of this function
     */
    private String toSymbol(Map<String, String> argumentBindings) {
        if (argumentBindings.isEmpty()) return name;

        StringBuilder ret = new StringBuilder();
        ret.append(name).append("@");
        for (Map.Entry<String,String> argumentBinding : argumentBindings.entrySet()) {
            ret.append(Long.toHexString(symbolCode(argumentBinding.getKey() + "=" + argumentBinding.getValue())));
            ret.append(".");
        }
        if (ret.toString().endsWith("."))
            ret.setLength(ret.length()-1);
        return ret.toString();
    }


    /**
     * Returns a more unique hash code than what Java's own {@link
     * String#hashCode()} method would produce.
     *
     * @param str The string to hash.
     * @return A 64 bit long hash code.
     */
    private static long symbolCode(String str) {
        try {
            MessageDigest md = java.security.MessageDigest.getInstance("SHA-1");
            byte[] buf = md.digest(Utf8.toBytes(str));
            if (buf.length >= 8) {
                long ret = 0;
                for (int i = 0; i < 8; ++i) {
                    ret = (ret << 8) + (buf[i] & 0xff);
                }
                return ret;
            }
        } catch (NoSuchAlgorithmException e) {
            throw new Error("java must always support SHA-1 message digest format", e);
        }
        return str.hashCode();
    }

    @Override
    public String toString() {
        return "function '" + name + "'";
    }
    
    /**
     * An instance of a serialization of this function, using a particular serialization context (by {@link
     * ExpressionFunction#expand})
     */
    public class Instance {

        private final String name;
        private final String expressionString;

        public Instance(String name, String expressionString) {
            this.name = name;
            this.expressionString = expressionString;
        }

        public String getName() {
            return name;
        }

        public String getExpressionString() {
            return expressionString;
        }

    }

}
